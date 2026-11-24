# Resilience4j Integration with Spring Boot HTTP Clients

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | Distributed Systems Reliability, RestClient / WebClient, AOP Fundamentals |
| **Estimated Reading Time** | 25 minutes |
| **Status** | Production-Ready |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | OpenJDK 21 LTS / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | Resilience4j 2.2, Micrometer 1.14, OpenJDK 21.0.2 |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Configuration](#4-syntax--configuration)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance & Security Considerations](#8-performance--security-considerations)
9. [Comparison: Circuit Breaker Patterns & Alternatives](#9-comparison-circuit-breaker-patterns--alternatives)
10. [Interview Questions & Deep Dives](#10-interview-questions--deep-dives)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

In a microservices architecture, network calls across boundaries will inevitably fail, slow down, or become unavailable. Without fault tolerance, a single slow downstream dependency can trigger a cascading failure: threads become blocked waiting on socket responses, thread pools saturate, and upstream callers fail, bringing down the entire ecosystem.

### The Problem
- **Cascading Failures**: A downstream service taking 30 seconds per request ties up all Tomcat worker threads in the calling service.
- **Thundering Herd**: Blindly retrying failed requests simultaneously overwhelms an already struggling downstream dependency.
- **Hystrix Deprecation**: Netflix Hystrix was officially deprecated years ago and lacks support for modern Java 21, reactive streams, and lightweight functional abstractions.

### The Solution: Resilience4j
Resilience4j is a lightweight, fault-tolerance library designed specifically for Java 17/21 and functional programming. It provides modular, composable resilience patterns:
1. **Circuit Breaker**: Stops executing requests when failure thresholds are exceeded, giving downstream services time to recover.
2. **Rate Limiter**: Caps requests per time window to prevent API quota breaches.
3. **Retry**: Automatically retries transient failures with exponential backoff and randomized jitter.
4. **TimeLimiter**: Enforces strict execution deadlines on asynchronous tasks.
5. **Bulkhead**: Limits concurrent calls to prevent one slow dependency from consuming all available execution capacity.

---

## 2. Core Concepts

### Mental Model: The Circuit Breaker State Machine
```
                             Success rate normal
                        ┌───────────────────────────┐
                        │                           │
                        ▼                           │
               ┌─────────────────┐                  │
               │     CLOSED      │                  │
               │  (Normal Flow)  │                  │
               └────────┬────────┘                  │
                        │ Failure rate > 50%        │
                        ▼                           │
               ┌─────────────────┐                  │
               │      OPEN       │                  │
               │ (Fails Fast /   │                  │
               │ Fallback Used)  │                  │
               └────────┬────────┘                  │
                        │ Wait duration elapsed     │
                        ▼                           │
               ┌─────────────────┐                  │
               │    HALF-OPEN    │                  │
               │ (Trial Requests)├──────────────────┘
               └────────┬────────┘   Trial requests succeed
                        │
                        │ Trial requests fail
                        ▼
                   Returns to OPEN
```

### Invariants
1. **Circuit Breaker States**:
   - **CLOSED**: Normal operation; calls proceed to the remote service. Call metrics are tracked in a sliding window (count-based or time-based).
   - **OPEN**: Remote service is failing; all calls are blocked immediately (`CallNotPermittedException`) and diverted to fallback methods without hitting the network.
   - **HALF-OPEN**: After a configured duration, trial calls are permitted to test whether the downstream dependency has recovered.
2. **Deterministic Fallback Signature**: A fallback method MUST match the target method's return type and parameter list, appending the thrown exception as the final parameter.
3. **Sliding Window Minimum**: The circuit breaker does NOT trip until the `minimumNumberOfCalls` threshold in the sliding window has been met.

---

## 3. How It Works Internally

### Aspect-Oriented Interception (`resilience4j-spring-boot3`)
1. Resilience4j registers Spring AOP aspects (`CircuitBreakerAspect`, `RetryAspect`, `RateLimiterAspect`, `BulkheadAspect`).
2. When a method annotated with `@CircuitBreaker(name = "backendA", fallbackMethod = "fallback")` is invoked, the aspect intercepts execution.
3. The aspect checks `CircuitBreaker.tryAcquirePermission()`. If OPEN, it immediately executes the fallback method.
4. If permitted, the method executes. On completion, the aspect records call duration and success/failure status in the `CircuitBreaker` sliding window ring buffer.
5. All transitions emit events to Micrometer `MeterRegistry` (`resilience4j.circuitbreaker.state`, `calls`).

---

## 4. Syntax & Configuration

### Dependency Setup (`pom.xml`)
```xml
<dependency>
    <groupId>io.github.resilience4j</groupId>
    <artifactId>resilience4j-spring-boot3</artifactId>
    <version>2.2.0</version>
</dependency>
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-aop</artifactId>
</dependency>
```

### Standard `application.yaml` Resilience4j Configuration
```yaml
resilience4j:
  circuitbreaker:
    instances:
      paymentService:
        sliding-window-type: COUNT_BASED
        sliding-window-size: 20 # Evaluates failure rate over last 20 calls
        minimum-number-of-calls: 10 # Must have 10 calls before calculating rate
        failure-rate-threshold: 50.0 # Open circuit if >= 50% fail
        slow-call-rate-threshold: 75.0 # Open circuit if >= 75% are slow
        slow-call-duration-threshold: 2000ms # Calls taking > 2s are slow
        wait-duration-in-open-state: 10000ms # Stay OPEN for 10s before HALF-OPEN
        permitted-number-of-calls-in-half-open-state: 5
        automatic-transition-from-open-to-half-open-enabled: true
        record-exceptions:
          - org.springframework.web.client.HttpServerErrorException
          - java.io.IOException
          - java.util.concurrent.TimeoutException
        ignore-exceptions:
          - com.example.exceptions.BusinessValidationException

  retry:
    instances:
      paymentService:
        max-attempts: 3
        wait-duration: 500ms
        enable-exponential-backoff: true
        exponential-backoff-multiplier: 2.0
        retry-exceptions:
          - java.io.IOException
          - org.springframework.web.client.HttpServerErrorException.ServiceUnavailable
```

---

## 5. Practical Examples

### Example 1: Annotated Service with CircuitBreaker, Retry, and Fallback

```java
package com.example.clients.resilience;

import io.github.resilience4j.circuitbreaker.annotation.CircuitBreaker;
import io.github.resilience4j.retry.annotation.Retry;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;
import org.springframework.web.client.RestClient;

import java.math.BigDecimal;
import java.util.UUID;

public record PaymentConfirmation(UUID transactionId, String status, String note) {}

@Service
public class ResilientPaymentClient {

    private static final Logger log = LoggerFactory.getLogger(ResilientPaymentClient.class);
    private final RestClient restClient;

    public ResilientPaymentClient(RestClient.Builder builder) {
        this.restClient = builder.baseUrl("https://payment-gateway.internal").build();
    }

    @CircuitBreaker(name = "paymentService", fallbackMethod = "processPaymentFallback")
    @Retry(name = "paymentService")
    public PaymentConfirmation executePayment(UUID accountId, BigDecimal amount) {
        log.info("Attempting payment for account {} of amount {}", accountId, amount);

        return restClient.post()
                .uri("/api/v1/charge")
                .body(new ChargePayload(accountId, amount))
                .retrieve()
                .body(PaymentConfirmation.class);
    }

    /**
     * Fallback method executed when Circuit is OPEN or Retries are exhausted.
     * Signature MUST match target method + final Throwable parameter!
     */
    public PaymentConfirmation processPaymentFallback(UUID accountId, BigDecimal amount, Throwable ex) {
        log.warn("Payment execution failed or circuit OPEN for account {}. Fallback triggered: {}", accountId, ex.getMessage());
        return new PaymentConfirmation(
                UUID.fromString("00000000-0000-0000-0000-000000000000"),
                "QUEUED_FOR_OFFLINE_PROCESSING",
                "Transaction queued safely during downstream payment gateway outage."
        );
    }

    record ChargePayload(UUID accountId, BigDecimal amount) {}
}
```

### Example 2: Programmatic CircuitBreaker Composition with `RestClient`

When annotations cannot be used (e.g. dynamic pipelines or non-Spring-managed beans):

```java
package com.example.clients.resilience;

import io.github.resilience4j.circuitbreaker.CircuitBreaker;
import io.github.resilience4j.circuitbreaker.CircuitBreakerRegistry;
import org.springframework.stereotype.Service;
import org.springframework.web.client.RestClient;

import java.util.UUID;
import java.util.function.Supplier;

@Service
public class ProgrammaticResilienceService {

    private final RestClient restClient;
    private final CircuitBreaker circuitBreaker;

    public ProgrammaticResilienceService(RestClient.Builder builder, CircuitBreakerRegistry registry) {
        this.restClient = builder.baseUrl("https://inventory.internal").build();
        this.circuitBreaker = registry.circuitBreaker("inventoryService");
    }

    public int checkStock(UUID itemId) {
        // Decorate Supplier with circuit breaker
        Supplier<Integer> decoratedSupplier = CircuitBreaker.decorateSupplier(
                circuitBreaker,
                () -> restClient.get()
                        .uri("/stock/{id}", itemId)
                        .retrieve()
                        .body(Integer.class)
        );

        // Execute with safe fallback on failure
        try {
            return decoratedSupplier.get();
        } catch (Exception ex) {
            return 0; // Safe default stock count on circuit open
        }
    }
}
```

### Example 3: Edge Case — Handling Rate Limiting with `RateLimiter`

```java
package com.example.clients.resilience;

import io.github.resilience4j.ratelimiter.annotation.RateLimiter;
import org.springframework.stereotype.Service;
import org.springframework.web.client.RestClient;

@Service
public class ThirdPartyGeoLocationService {

    private final RestClient restClient;

    public ThirdPartyGeoLocationService(RestClient.Builder builder) {
        this.restClient = builder.baseUrl("https://api.geolocate.io").build();
    }

    /**
     * Enforces external provider limit: max 10 requests per second.
     * Subsequent concurrent callers wait up to timeout-duration before being rejected.
     */
    @RateLimiter(name = "geoLocationApi", fallbackMethod = "rateLimitFallback")
    public String resolveCoordinates(String ipAddress) {
        return restClient.get()
                .uri("/lookup/{ip}", ipAddress)
                .retrieve()
                .body(String.class);
    }

    public String rateLimitFallback(String ipAddress, Throwable ex) {
        return "DEFAULT_REGION_US_EAST";
    }
}
```

### Example 4: Anti-Pattern vs Production Fix

#### Anti-Pattern: Blind Retries on Non-Idempotent POST Requests without Jitter
```yaml
# ANTI-PATTERN: Retrying all exceptions blindly without backoff
resilience4j:
  retry:
    instances:
      badRetry:
        max-attempts: 5
        wait-duration: 0ms # Retries immediately! (Thundering Herd attack against failing service)
        # Retries HTTP 400 Bad Request! (Will never succeed, wastes CPU)
```

#### Production Fix: Selective Retries on Transient Errors with Exponential Backoff
```yaml
# PRODUCTION FIX: Retry only transient server errors and network disconnects with backoff
resilience4j:
  retry:
    instances:
      goodRetry:
        max-attempts: 3
        wait-duration: 500ms
        enable-exponential-backoff: true
        exponential-backoff-multiplier: 2.0
        randomize-wait: true # Adds randomized jitter to spread retry load
        retry-exceptions:
          - java.io.IOException
          - org.springframework.web.client.HttpServerErrorException.ServiceUnavailable
          - org.springframework.web.client.HttpServerErrorException.GatewayTimeout
        ignore-exceptions:
          - org.springframework.web.client.HttpClientErrorException # 4xx errors are permanent client mistakes
```

### Example 5: High-Performance Bulkhead Isolation for Slow Downstream APIs

```yaml
# application.yaml
resilience4j:
  bulkhead:
    instances:
      externalReportService:
        max-concurrent-calls: 10 # Limits concurrent calls to 10; rejects with BulkheadFullException
        max-wait-duration: 200ms
```

```java
package com.example.clients.resilience;

import io.github.resilience4j.bulkhead.annotation.Bulkhead;
import org.springframework.stereotype.Service;
import org.springframework.web.client.RestClient;

@Service
public class ReportingBatchService {

    private final RestClient restClient;

    public ReportingBatchService(RestClient.Builder builder) {
        this.restClient = builder.baseUrl("https://slow-reports.internal").build();
    }

    /**
     * Bulkhead prevents slow reporting calls from occupying more than 10 concurrent threads,
     * protecting core application capacity.
     */
    @Bulkhead(name = "externalReportService", fallbackMethod = "bulkheadFallback")
    public byte[] generatePdfReport(String reportId) {
        return restClient.get()
                .uri("/pdf/{id}", reportId)
                .retrieve()
                .body(byte[].class);
    }

    public byte[] bulkheadFallback(String reportId, Throwable ex) {
        return new byte[0]; // Empty payload or alert message
    }
}
```

---

## 6. Best Practices

1. **Always Set `minimumNumberOfCalls`**: Avoid premature circuit tripping. Do not calculate failure rate until at least 10 or 20 calls have been made in the sliding window.
2. **Never Retry 4xx Client Errors**: HTTP 400 (Bad Request), 401 (Unauthorized), and 404 (Not Found) are permanent client errors; retrying them wastes CPU and network resources.
3. **Always Add Jitter to Retries**: Set `randomize-wait: true` to prevent synchronized thundering herd spikes on downstream services.
4. **Order Aspects Correctly**: Resilience4j executes annotations in a defined priority order: `Bulkhead` $\rightarrow$ `TimeLimiter` $\rightarrow$ `RateLimiter` $\rightarrow$ `CircuitBreaker` $\rightarrow$ `Retry`.
5. **Monitor Circuit Breakers via Actuator**: Enable `/actuator/circuitbreakers` and `/actuator/metrics` to alert ops teams when circuits open.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Mismatched Fallback Signature** | Spring throws `NoSuchMethodException` at runtime if parameters or return types do not match exactly. | Ensure fallback signature matches target method + `Throwable` at the end. |
| **Retrying Non-Idempotent Operations** | Blindly retrying a `POST /charge` request charges the user multiple times during partial network drops. | Use idempotent endpoints with `Idempotency-Key` or restrict retries to GET/PUT. |
| **Circuit Breaker Not Opening on Slow Calls** | Calls hang for 30s but do not throw exceptions, so failure rate stays 0%. | Configure `slow-call-rate-threshold` and `slow-call-duration-threshold`. |
| **Calling Annotated Method Internally (`this.call()`)** | Bypasses Spring AOP proxy; CircuitBreaker and Retry aspects are never executed! | Invoke methods through an injected bean reference or self-injected proxy. |
| **Missing `spring-boot-starter-aop`** | Annotations (`@CircuitBreaker`) are silently ignored; calls hit the network directly. | Include `spring-boot-starter-aop` in `pom.xml`. |
| **Opening Circuit on Business Exceptions** | Business validation exceptions (`InvalidCouponException`) trip the circuit breaker, stopping valid traffic. | Add domain business exceptions to `ignore-exceptions`. |

---

## 8. Performance & Security Considerations

### Sliding Window Memory Footprint
Resilience4j's count-based sliding window uses a circular array (ring buffer) that is extremely memory efficient (~few KB per instance). Time-based sliding windows use a ring buffer of buckets. Prefer count-based windows unless your call volume is highly erratic.

### Fallback Information Disclosure
Fallback methods should return safe cached defaults or generic queued confirmations. Never return raw exception stack traces or internal infrastructure details inside fallback responses.

---

## 9. Comparison: Circuit Breaker Patterns & Alternatives

| Dimension | Resilience4j | Netflix Hystrix (Deprecated) | Spring Cloud Circuit Breaker |
| :--- | :--- | :--- | :--- |
| **Java Compatibility** | Modern Java 17 / 21+ | Legacy Java 8 | Abstraction Layer (Delegates to Resilience4j) |
| **Concurrency Model** | In-thread / Functional / Virtual Threads | Thread-pool isolation (Heavy overhead) | Depends on underlying implementation |
| **Maintenance Status** | Active (Industry Standard) | Deprecated | Active |
| **Footprint** | Lightweight (No dependencies besides Vavr) | Heavy (Archaius, RxJava) | Minimal wrapper |
| **Actuator & Micrometer**| Native Integration | Legacy Turbine / Hystrix Dashboard | Native Integration |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: What are the three states of a Circuit Breaker and how does state transition occur?</strong></summary>

**Answer:**
1. **CLOSED**: Normal state. Requests proceed to the backend. Failure and slow call rates are recorded in a sliding window. If the failure rate exceeds `failure-rate-threshold` after `minimumNumberOfCalls`, the circuit transitions to **OPEN**.
2. **OPEN**: The remote service is considered down. All incoming calls fail immediately with `CallNotPermittedException` (invoking fallback methods) without hitting the network. After `wait-duration-in-open-state` elapses, the circuit transitions to **HALF-OPEN**.
3. **HALF-OPEN**: A small batch of trial calls (`permitted-number-of-calls-in-half-open-state`) are permitted to hit the backend. If the trial calls succeed, the circuit transitions back to **CLOSED**. If any trial calls fail, it transitions back to **OPEN** for another duration.
</details>

<details>
<summary><strong>Q2: Why does invoking an annotated method from within the same class (`this.myMethod()`) fail to trigger Resilience4j?</strong></summary>

**Answer:**
Resilience4j annotations (`@CircuitBreaker`, `@Retry`) rely on Spring AOP dynamic proxies. When an external bean calls the method, it goes through the proxy, which executes the aspect logic. When a method calls another method within the same class using `this.myMethod()`, the call bypasses the Spring proxy and executes directly against the local target instance, silently bypassing all aspects.
</details>

<details>
<summary><strong>Q3: What are the strict rules for the method signature of a Resilience4j fallback method?</strong></summary>

**Answer:**
1. Must have the exact same return type as the original method.
2. Must have the exact same parameter list in the same order as the original method.
3. Must accept a final additional parameter of type `Throwable` (or a specific exception subclass).
4. Must reside in the same class (or a shared fallback class configured via `fallbackMethod`).
</details>

<details>
<summary><strong>Q4: What is the difference between a Count-Based and a Time-Based sliding window?</strong></summary>

**Answer:**
- **Count-Based**: Tracks the last $N$ calls (e.g. 100 calls) in an array. The failure rate is calculated as: `(failedCalls / totalWindowCalls) * 100`.
- **Time-Based**: Tracks calls over the last $N$ seconds (e.g. 60 seconds) using a circular array of buckets. Useful for systems with consistent request velocity where time intervals provide a better picture of degradation.
</details>

<details>
<summary><strong>Q5: What is the purpose of the Bulkhead pattern in Resilience4j?</strong></summary>

**Answer:**
The Bulkhead pattern isolates resources to prevent a slow service from consuming all application resources. Resilience4j provides two bulkhead types:
- **SemaphoreBulkhead**: Limits the number of concurrent executions permitted on the calling thread.
- **ThreadPoolBulkhead**: Uses a dedicated bounded thread pool and queue to isolate calls onto separate worker threads.
</details>

<details>
<summary><strong>Q6: Why is adding randomized jitter to retry mechanisms critical in distributed systems?</strong></summary>

**Answer:**
If 1,000 clients experience a failure at time $T$, and all retry with a fixed delay of 1 second, all 1,000 clients will hit the recovering service simultaneously at $T + 1s$, triggering a "thundering herd" or "retry storm" that knocks the service offline again. Adding randomized jitter spreads retries evenly across a time window (e.g. between 800ms and 1500ms), smoothing out incoming traffic spikes.
</details>

<details>
<summary><strong>Q7: How does Resilience4j integrate with Spring Boot Actuator?</strong></summary>

**Answer:**
Resilience4j exposes dedicated Actuator endpoints: `/actuator/circuitbreakers`, `/actuator/circuitbreakerevents`, `/actuator/retries`. It also publishes metrics to Micrometer, exposing gauges for circuit state, buffered calls, failed calls, and slow calls for Prometheus and Grafana dashboards.
</details>

<details>
<summary><strong>Q8: What exception is thrown when a call is made to an OPEN circuit breaker?</strong></summary>

**Answer:**
`io.github.resilience4j.circuitbreaker.CallNotPermittedException`. If a fallback method is defined, this exception is intercepted and passed to the fallback method.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Basic CircuitBreaker YAML Configuration</strong></summary>

**Task**: Write the YAML configuration for a circuit breaker `orderService` that opens if 50% of the last 10 calls fail, staying open for 5 seconds.

```yaml
# Solution:
resilience4j:
  circuitbreaker:
    instances:
      orderService:
        sliding-window-size: 10
        minimum-number-of-calls: 10
        failure-rate-threshold: 50.0
        wait-duration-in-open-state: 5000ms
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Annotated Method with Matching Fallback</strong></summary>

**Task**: Write a service method `@CircuitBreaker(name = "itemService", fallbackMethod = "fallback") public String getItem(Long id)` and its corresponding fallback method.

```java
// Solution:
@Service
public class ItemService {
    @CircuitBreaker(name = "itemService", fallbackMethod = "fallback")
    public String getItem(Long id) {
        // remote call
        return "Item " + id;
    }

    public String fallback(Long id, Throwable t) {
        return "Fallback Item " + id;
    }
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Exponential Backoff Retry Configuration</strong></summary>

**Task**: Configure a retry instance `authService` for 3 attempts, initial delay of 200ms, multiplier of 2.0, with randomized jitter.

```yaml
# Solution:
resilience4j:
  retry:
    instances:
      authService:
        max-attempts: 3
        wait-duration: 200ms
        enable-exponential-backoff: true
        exponential-backoff-multiplier: 2.0
        randomize-wait: true
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Ignore Business Exceptions in CircuitBreaker</strong></summary>

**Task**: Configure a circuit breaker so that `UserNotFoundException` does NOT count as a failure towards opening the circuit.

```yaml
# Solution:
resilience4j:
  circuitbreaker:
    instances:
      userService:
        ignore-exceptions:
          - com.example.exceptions.UserNotFoundException
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Custom State Transition Event Listener</strong></summary>

**Task**: Programmatically register an event consumer on the `CircuitBreakerRegistry` that logs an alert and triggers a Slack notification webhook whenever any circuit breaker transitions from `CLOSED` to `OPEN`.
</details>

<details>
<summary><strong>Exercise 6 (Hard): Composite Pipeline: RateLimiter + CircuitBreaker + Retry</strong></summary>

**Task**: Build a service combining `@RateLimiter(5 req/s)`, `@CircuitBreaker`, and `@Retry(3 attempts)` on a `RestClient` call, writing an automated JUnit 5 test verifying that rate limits, circuit breaks, and retries fire in correct order.
</details>

---

## 12. Summary & Quick Reference

- **Core Module**: `resilience4j-spring-boot3` + `spring-boot-starter-aop`.
- **States**: `CLOSED` (Normal) $\rightarrow$ `OPEN` (Fail Fast) $\rightarrow$ `HALF-OPEN` (Trial).
- **Fallback Rule**: Same parameters + `Throwable ex` as final parameter.
- **Retry Rule**: Never retry 4xx errors; always add exponential backoff and jitter.
- **Monitoring**: Inspect via `/actuator/circuitbreakers` and Micrometer metrics.

---

## 13. Further Reading

- [Resilience4j Official Documentation](https://resilience4j.readme.io/docs)
- [Release It! Second Edition (Michael Nygard)](https://pragprog.com/titles/mnee2/release-it-second-edition/)
- [Spring Cloud Circuit Breaker Reference](https://docs.spring.io/spring-cloud-circuitbreaker/docs/current/reference/html/)
- [Martin Fowler: Circuit Breaker Pattern](https://martinfowler.com/bliki/CircuitBreaker.html)
