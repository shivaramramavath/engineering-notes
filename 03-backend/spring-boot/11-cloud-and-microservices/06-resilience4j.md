# Fault Tolerance with Resilience4j: Circuit Breakers, Retries, and Bulkheads

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | `io.github.resilience4j:resilience4j-spring-boot3:2.2.x` |

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
- [8. Performance & Security Notes](#8-performance--security-notes)
- [9. Comparison / When to Use What](#9-comparison--when-to-use-what)
- [10. Interview Questions](#10-interview-questions)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

### The Problem
In distributed microservice architectures, networks are inherently unreliable and downstream third-party services (payment gateways, shipping APIs, inventory databases) frequently experience degradation, slow response times, or complete outages. In naive systems, when an upstream service calls a failing downstream service, client worker threads hang waiting on TCP timeouts. In high-traffic systems, this consumes the entire application thread pool in seconds, causing **cascading system failures** where a slowdown in a non-critical peripheral service takes down the core application.

### The Evolution
Historically, Netflix Hystrix pioneered circuit breaking in Java. However, Hystrix relied on heavy thread pools (`HystrixThreadPool`), complex archaius configurations, and has been in maintenance mode since 2018. **Resilience4j** is a modern, lightweight fault-tolerance library designed specifically for Java 8+ and functional programming. Unlike Hystrix, Resilience4j uses modular, non-blocking higher-order decorators and Ring Bit Buffers without forcing thread context switching.

### Relevance in Modern Engineering
Resilience4j is the official fault-tolerance standard for Spring Boot 3. It provides five foundational resilience patterns: **CircuitBreaker**, **Retry**, **RateLimiter**, **TimeLimiter**, and **Bulkhead**, with declarative annotations (`@CircuitBreaker`), comprehensive Actuator metric instrumentation, and automated Prometheus alerting.

---

## 2. Core Concepts

- **CircuitBreaker**: A finite state machine protecting downstream resources.
  - *CLOSED*: Normal operation; requests pass through. Failures are measured against a sliding window threshold.
  - *OPEN*: Threshold exceeded (e.g. $> 50\%$ failures); calls fail fast immediately without hitting downstream network.
  - *HALF_OPEN*: Trial state after a cooldown period; allows a small trial batch of requests through to check if the downstream service has recovered.
- **Retry**: Automatically re-executes failing operations with configurable attempts and exponential backoff.
- **RateLimiter**: Restricts the maximum number of requests executed within a defined time frame (e.g., max 100 req/sec).
- **TimeLimiter**: Enforces strict timeout ceilings on asynchronous or reactive executions (e.g., timeout after 2.5s).
- **Bulkhead**: Limits the maximum number of concurrent executions to prevent a single downstream service from monopolizing all server capacity (Semaphore-based or ThreadPool-based).
- **Fallback**: A graceful degradation method executed when the circuit is open or when an exception occurs.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                     Resilience4j CircuitBreaker State Machine                     |
|                                                                                   |
|                    +------------------------------------+                         |
|                    |               CLOSED               |                         |
|                    | Normal Operation (Calls pass)      |                         |
|                    +------------------------------------+                         |
|                               |             ^                                     |
|           Failure Rate >= 50% |             | Trial Success Rate >= 50%           |
|           (Over Sliding Window)             |                                     |
|                               v             |                                     |
|                    +------------------------------------+                         |
|                    |                OPEN                |                         |
|                    | Fail Fast! Returns Fallback        |                         |
|                    +------------------------------------+                         |
|                               |                                                   |
|                               | waitDurationInOpenState (e.g. 10s) expires        |
|                               v                                                   |
|                    +------------------------------------+                         |
|                    |             HALF_OPEN              |                         |
|                    | Allows N Trial Calls (e.g. 5 calls)|                         |
|                    +------------------------------------+                         |
|                               |                                                   |
|                               +---------------------------------------------------+
|                                       Trial Failure Rate >= 50% (Back to OPEN)    |
+-----------------------------------------------------------------------------------+
```

Resilience4j tracks execution metrics using a **Sliding Window** (either Count-based e.g., last 100 calls, or Time-based e.g., last 60 seconds). When a method annotated with `@CircuitBreaker` is invoked, Resilience4j inspects the circuit state. If `OPEN`, it immediately bypasses the method and routes execution to the configured fallback. If `CLOSED`, it delegates to the real method, evaluates whether the result is a success or exception, and updates the circular bit-buffer. If the failure rate or slow call rate exceeds the configured threshold, the state machine transitions immediately to `OPEN`.

---

## 4. Syntax & Usage

### Maven Dependency
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

### Essential Application Configuration (`application.yml`)
```yaml
resilience4j:
  circuitbreaker:
    instances:
      paymentServiceCB:
        sliding-window-type: COUNT_BASED
        sliding-window-size: 20
        minimum-number-of-calls: 10
        failure-rate-threshold: 50.0
        slow-call-rate-threshold: 50.0
        slow-call-duration-threshold: 2000ms
        wait-duration-in-open-state: 10000ms
        permitted-number-of-calls-in-half-open-state: 5
        automatic-transition-from-open-to-half-open-enabled: true
```

---

## 5. Practical Examples

### Example 1: Declarative `@CircuitBreaker` with Graceful Fallback
```java
package com.example.platform.resilience.service;

import io.github.resilience4j.circuitbreaker.annotation.CircuitBreaker;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;
import org.springframework.web.client.RestClient;

import java.util.UUID;

public record PaymentResult(String transactionId, String status, boolean isFallback) {}

@Service
public class ResilientPaymentService {

    private static final Logger log = LoggerFactory.getLogger(ResilientPaymentService.class);
    private final RestClient restClient;

    public ResilientPaymentService(RestClient.Builder builder) {
        this.restClient = builder.baseUrl("https://payment-gateway.internal").build();
    }

    // CircuitBreaker intercepts execution; on failure or open circuit, routes to paymentFallback
    @CircuitBreaker(name = "paymentServiceCB", fallbackMethod = "paymentFallback")
    public PaymentResult authorizePayment(String orderId, double amount) {
        log.info("Attempting payment gateway authorization for order: {}", orderId);

        // May throw ResourceAccessException (timeout) or HttpServerErrorException (5xx)
        return restClient.post()
                .uri("/v1/charge")
                .body(new ChargeRequest(orderId, amount))
                .retrieve()
                .body(PaymentResult.class);
    }

    // Fallback signature MUST match the original method's parameters + Throwable as the final parameter
    public PaymentResult paymentFallback(String orderId, double amount, Throwable t) {
        log.warn("Payment Circuit Breaker TRIGGERED for order [{}]. Error: {}. Returning degraded fallback.",
                orderId, t.getMessage());

        // Queue transaction for offline reconciliation rather than failing user checkout
        return new PaymentResult("offline_" + UUID.randomUUID(), "PENDING_OFFLINE_REVIEW", true);
    }

    record ChargeRequest(String orderId, double amount) {}
}
```

### Example 2: Chaining Retry and Bulkhead Annotations
```java
package com.example.platform.resilience.service;

import io.github.resilience4j.bulkhead.annotation.Bulkhead;
import io.github.resilience4j.circuitbreaker.annotation.CircuitBreaker;
import io.github.resilience4j.retry.annotation.Retry;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;

@Service
public class ResilientInventoryService {

    private static final Logger log = LoggerFactory.getLogger(ResilientInventoryService.class);

    // Execution Order: Retry -> CircuitBreaker -> Bulkhead -> Real Method
    @Bulkhead(name = "inventoryBulkhead", type = Bulkhead.Type.SEMAPHORE)
    @CircuitBreaker(name = "inventoryCB")
    @Retry(name = "inventoryRetry", fallbackMethod = "inventoryFallback")
    public boolean reserveStock(String sku, int quantity) {
        log.info("Reserving stock for SKU: {}, Quantity: {}", sku, quantity);
        // External call to warehouse microservice
        return true;
    }

    public boolean inventoryFallback(String sku, int quantity, Throwable t) {
        log.error("All retries exhausted or bulkhead saturated for SKU: {}. Error: {}", sku, t.getMessage());
        return false;
    }
}
```

### Example 3: Resilience4j Configuration for Retries and Bulkheads
```yaml
# application.yml
resilience4j:
  retry:
    instances:
      inventoryRetry:
        max-attempts: 3
        wait-duration: 500ms
        enable-exponential-backoff: true
        exponential-backoff-multiplier: 2.0
        retry-exceptions:
          - java.net.SocketTimeoutException
          - org.springframework.web.client.ResourceAccessException
        ignore-exceptions:
          - java.lang.IllegalArgumentException
  bulkhead:
    instances:
      inventoryBulkhead:
        max-concurrent-calls: 25 # Max 25 concurrent threads allowed to enter
        max-wait-duration: 50ms  # Reject fast if all 25 slots occupied
```

### Example 4: The Fallback Method Signature Mismatch Anti-Pattern and Fix
```java
// ANTI-PATTERN: Defining a fallback method with mismatched argument types or missing the Throwable parameter!
// @CircuitBreaker(name = "badCB", fallbackMethod = "myFallback")
// public User getUser(String id) { ... }
// public User myFallback(String id) { ... } // CRASHES! NoSuchMethodException: myFallback(String, Throwable)!

// FIX: Always append Throwable (or specific Exception) as the final argument in the fallback method
public User myFallback(String id, Throwable t) {
    return new User(id, "Anonymous Guest");
}
```

### Example 5: Programmatic Functional Decorator Pipeline
```java
package com.example.platform.resilience.functional;

import io.github.resilience4j.circuitbreaker.CircuitBreaker;
import io.github.resilience4j.circuitbreaker.CircuitBreakerRegistry;
import io.github.resilience4j.retry.Retry;
import io.github.resilience4j.retry.RetryRegistry;
import org.springframework.stereotype.Service;

import java.util.function.Supplier;

@Service
public class ProgrammaticResilienceService {

    private final CircuitBreaker circuitBreaker;
    private final Retry retry;

    public ProgrammaticResilienceService(CircuitBreakerRegistry cbRegistry, RetryRegistry retryRegistry) {
        this.circuitBreaker = cbRegistry.circuitBreaker("dynamicServiceCB");
        this.retry = retryRegistry.retry("dynamicServiceRetry");
    }

    public String executeWithResilience(String input) {
        Supplier<String> baseCall = () -> callRemoteService(input);

        // Decorate with CircuitBreaker and Retry programmatically without AOP annotations
        Supplier<String> decorated = CircuitBreaker.decorateSupplier(circuitBreaker, baseCall);
        decorated = Retry.decorateSupplier(retry, decorated);

        return io.vavr.control.Try.ofSupplier(decorated)
                .recover(throwable -> "Fallback Value for: " + throwable.getMessage())
                .get();
    }

    private String callRemoteService(String input) {
        return "Processed: " + input;
    }
}
```

---

## 6. Best Practices

1. **Always Set `minimum-number-of-calls`**: Prevent the circuit breaker from tripping on a single isolated failure; require at least 10-20 calls before calculating failure percentages.
2. **Configure Slow Call Thresholds**: Timeouts often degrade systems worse than outright failures. Configure `slow-call-duration-threshold` (e.g., 2s) and `slow-call-rate-threshold` to trip the circuit on hanging services.
3. **Always Include `Throwable` in Fallbacks**: The fallback method signature must match the original method's parameter list with a `Throwable` (or specific exception) appended as the last parameter.
4. **Use Bulkheads on High-Volume Endpoints**: Apply semaphore Bulkheads (`max-concurrent-calls`) to prevent slow external dependencies from consuming all Tomcat/Netty request worker threads.
5. **Classify Non-Retryable Exceptions**: Never retry validation errors or client 4xx exceptions (`IllegalArgumentException`, `HttpClientErrorException`); retry only transient network/5xx failures.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Missing `Throwable` parameter in fallback method | Throws runtime `NoSuchMethodException` when circuit trips | Append `Throwable t` as the final parameter of the fallback method. |
| Forgetting `spring-boot-starter-aop` dependency | Declarative annotations (`@CircuitBreaker`) are silently ignored | Include `org.springframework.boot:spring-boot-starter-aop`. |
| Internal method invocation (`this.call()`) | Bypasses Spring CGLIB AOP proxy; circuit breaker never executes | Extract method to a separate Spring `@Service` bean. |
| Setting sliding window size smaller than minimum calls | Circuit cannot evaluate thresholds properly | Ensure `sliding-window-size >= minimum-number-of-calls`. |
| Retrying non-idempotent operations (e.g., non-idempotent HTTP POST) | Can charge customers twice or create duplicate database entries | Use retries only on idempotent operations or with idempotency keys. |
| Placing `@CircuitBreaker` on `private` methods | Spring AOP proxies cannot intercept private methods | Make annotated methods `public`. |

---

## 8. Performance & Security Notes

- **Zero Thread-Switching Overhead**: Resilience4j decorators execute directly on the calling thread using atomic state updates and Ring Bit Buffers, consuming negligible CPU and zero extra thread memory.
- **Actuator Health Integration**: Expose circuit breaker state in `/actuator/health` so orchestrators (Kubernetes) and alerting systems detect degraded upstream services immediately.
- **Information Exposure in Fallbacks**: Never return raw exception stack traces or internal connection URLs inside user-facing fallback payloads.

---

## 9. Comparison / When to Use What

| Pattern | CircuitBreaker | Retry | RateLimiter | Bulkhead |
|---|---|---|---|---|
| **Goal** | Stop calls to failing service | Overcome momentary glitches | Limit incoming request rate | Limit concurrent execution |
| **Trigger** | High failure or slow call rate | Specific exception thrown | Token count exhausted | Concurrency ceiling reached |
| **Duration** | Stays open for cooldown window | Immediate backoff delays | Blocks or rejects per second | Rejects immediately when full |
| **Best For** | Outages, service crashes | Network blips, 503 errors | API rate limits, DoS defense| Isolating thread capacity |

---

## 10. Interview Questions

<details>
<summary>1. What is the Circuit Breaker pattern and how does Resilience4j implement it?</summary>
The Circuit Breaker pattern protects systems from cascading failures by monitoring calls to external dependencies. Resilience4j implements it as a three-state state machine (CLOSED, OPEN, HALF_OPEN) using a circular sliding window (Count-based or Time-based). If failures or slow calls exceed a configured threshold, the circuit enters OPEN and fails fast immediately, shielding downstream services and returning a fallback without blocking.
</details>

<details>
<summary>2. What is the difference between a Count-Based and a Time-Based sliding window?</summary>
A Count-Based sliding window evaluates metrics over the last $N$ calls (e.g., the last 100 requests). A Time-Based sliding window evaluates metrics over the last $N$ seconds (e.g., the last 60 seconds), storing outcome counts in discrete epoch buckets.
</details>

<details>
<summary>3. What is a Bulkhead and why is it named after ship construction?</summary>
In shipbuilding, bulkheads are watertight compartments that prevent the entire ship from sinking if a single section is punctured. In software, a Bulkhead isolates resources (threads or concurrent slots) allocated to a specific downstream service. If that service hangs, only its allocated bulkhead capacity is consumed, leaving the rest of the application's threads available to serve other requests.
</details>

<details>
<summary>4. Why did Resilience4j replace Netflix Hystrix as the Spring Cloud standard?</summary>
Netflix placed Hystrix into maintenance mode in 2018. Hystrix forced calls into dedicated thread pools (`HystrixCommand`), adding heavy context switching and memory overhead. Resilience4j is modular, lightweight, built for Java 8 functional programming, operates on the calling thread via non-blocking Ring Bit Buffers, and integrates natively with Spring Boot 3 Actuator and Micrometer.
</details>

<details>
<summary>5. What are the rules for creating a valid fallback method in Resilience4j?</summary>
1. Must have the exact same return type as the original method.
2. Must have the exact same parameter list as the original method.
3. Must append a `Throwable` (or a specific exception subclass) as the final parameter.
4. Must reside in the same class (or a fallback factory).
</details>

<details>
<summary>6. How does the HALF_OPEN state work in Resilience4j?</summary>
When the `wait-duration-in-open-state` cooldown expires, the circuit transitions from OPEN to HALF_OPEN. In this state, it permits a limited number of trial calls (`permitted-number-of-calls-in-half-open-state`). If the trial calls succeed, the circuit transitions back to CLOSED; if they fail, it returns to OPEN for another cooldown period.
</details>

<details>
<summary>7. What is the default annotation execution order when combining @CircuitBreaker, @Retry, and @Bulkhead?</summary>
By default, the execution order is:
`Retry` $\to$ `CircuitBreaker` $\to$ `RateLimiter` $\to$ `TimeLimiter` $\to$ `Bulkhead` $\to$ Target Method.
This order ensures retries happen before the circuit breaker evaluates failure rates, and the bulkhead protects the final invocation.
</details>

<details>
<summary>8. Why should CircuitBreakers also monitor "Slow Calls" in addition to failures?</summary>
Because slow, hanging calls are often more dangerous than immediate 500 errors. A service that takes 30 seconds to respond holds server threads and database connections open, rapidly exhausting the caller's thread pool and taking down the whole system. Tripping the circuit on slow calls prevents thread pool starvation.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1 (Easy): Configure CircuitBreaker in application.yml</summary>
Write a configuration for a circuit breaker named `"userServiceCB"` with 50% failure rate threshold and 10-second wait duration.
</details>

<details>
<summary>Exercise 2 (Easy): Annotate method with @CircuitBreaker and fallback</summary>
Annotate a service method with `@CircuitBreaker` and provide a fallback method returning an empty list.
</details>

<details>
<summary>Exercise 3 (Medium): Configure Retry with exponential backoff</summary>
Configure Resilience4j retry for 3 attempts with initial delay 500ms and multiplier 2.0 ignoring `IllegalArgumentException`.
</details>

<details>
<summary>Exercise 4 (Medium): Build a Semaphore Bulkhead</summary>
Annotate a heavy report generation method with `@Bulkhead` allowing at most 5 concurrent calls.
</details>

<details>
<summary>Exercise 5 (Hard): Programmatic functional composition with Vavr Try</summary>
Write a service method that programmatically decorates a lambda with both `CircuitBreaker` and `Retry` and recovers using Vavr `Try`.
</details>

<details>
<summary>Exercise 6 (Hard): Integration test testing circuit breaker state transitions</summary>
Write a `@SpringBootTest` test that simulates 10 consecutive failures, asserts that the CircuitBreaker transitions to `OPEN`, and verifies that subsequent calls fail fast to fallback.
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                         Resilience4j Quick Reference                            |
+---------------------------------------------------------------------------------+
| Dependency:         io.github.resilience4j:resilience4j-spring-boot3            |
| Annotations:        @CircuitBreaker, @Retry, @Bulkhead, @RateLimiter            |
| Circuit States:     CLOSED (Pass) -> OPEN (Fail fast) -> HALF_OPEN (Trial)      |
| Fallback Signature: ReturnType methodName(OriginalParams..., Throwable t)      |
| Metric Tuning:      sliding-window-size, minimum-number-of-calls, failure-rate  |
| Slow Call Rule:     Always set slow-call-duration-threshold to prevent hangs   |
| Invariant:          Never call annotated methods internally (AOP proxy bypass)  |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Official Resilience4j Documentation](https://resilience4j.readme.io/)
- [Spring Cloud CircuitBreaker Reference](https://docs.spring.io/spring-cloud-circuitbreaker/reference/)
- [Release It! 2nd Edition (Michael T. Nygard)](https://pragprog.com/titles/mnee2/release-it-second-edition/)
