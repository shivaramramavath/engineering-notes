# Resilient Retry and Backoff Patterns in Java

> Fault tolerance architectures, exponential backoff, full/decorrelated jitter algorithms, transient failure discrimination, and production Java retry engines.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [`14-concurrency/00_threads-and-runnables.md`](../14-concurrency/00_threads-and-runnables.md), [`06-exceptions-and-debugging/00_exception-hierarchy.md`](../06-exceptions-and-debugging/00_exception-hierarchy.md) |
| **Time to Read** | 35 minutes |
| **Status** | Verified (Java 8 to 25 LTS) |
| **Standard** | Java SE 21 / 25 LTS |

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

In distributed cloud networks, transient failures are inevitable: packet loss, microsecond socket resets, database failovers, GC pause blips, and load balancer re-routing. If client services immediately fail on transient errors, overall system availability plummets.

However, naive retries (e.g., immediate tight retry loops) cause **Retry Storms**: thousands of failed clients hammer a struggling downstream service in unison, guaranteeing total cascade failure.

A resilient retry system combines:
1. **Transient Fault Classification**: Retrying only recoverable errors (e.g. HTTP 503, connection timeouts), never terminal business errors (e.g. HTTP 400, 404, auth failures).
2. **Exponential Backoff**: Successively doubling wait intervals.
3. **Randomized Jitter**: Desynchronizing retry waves across concurrent clients.
4. **Idempotency Verification**: Ensuring repeated calls produce identical side effects.

---

## 2. Core Concepts

### 2.1 Backoff Formulations

1. **Fixed Delay**:
   $$\text{delay} = C$$
   Vulnerable to resonance waves where all failing clients retry simultaneously at constant intervals.

2. **Exponential Backoff**:
   $$\text{delay}_n = \min(M, B \cdot 2^n)$$
   Where $B$ is the initial base delay, $n$ is the retry attempt index, and $M$ is the maximum backoff ceiling.

3. **Jitter Algorithms (Amazon AWS Research)**:
   - **Full Jitter**: Uniformly random distribution from 0 to calculated backoff:
     $$\text{delay} = \text{random}(0, \min(M, B \cdot 2^n))$$
   - **Equal Jitter**: Guaranteed half delay + randomized remainder:
     $$\text{half} = \frac{\min(M, B \cdot 2^n)}{2}, \quad \text{delay} = \text{half} + \text{random}(0, \text{half})$$
   - **Decorrelated Jitter**: Dynamic interval calculated from previous delay:
     $$\text{delay}_n = \min(M, \text{random}(B, \text{delay}_{n-1} \cdot 3))$$

---

## 3. How It Works Internally

### 3.1 Desynchronization via Jitter

```
WITHOUT JITTER (Deterministic Exponential Backoff):
Time: 0s    1s        3s              7s
Node A: [X] -> Wait 1s -> [X] -> Wait 2s -> [X] -> Wait 4s -> [X] (Coordinated Hammer!)
Node B: [X] -> Wait 1s -> [X] -> Wait 2s -> [X] -> Wait 4s -> [X]
Node C: [X] -> Wait 1s -> [X] -> Wait 2s -> [X] -> Wait 4s -> [X]
Result: Massive periodic traffic spikes crash the recovery of downstream service.

WITH FULL JITTER:
Node A: [X] -- Wait 0.4s --> [X] ----- Wait 1.8s -----> [X]
Node B: [X] ---- Wait 0.9s ----> [X] - Wait 0.6s -> [X]
Node C: [X] - Wait 0.2s -> [X] -------- Wait 2.1s --------> [X]
Result: Traffic smoothly spreads over time; server recovers cleanly.
```

---

## 4. Syntax & Usage

### 4.1 Java Record Retry Configuration

```java
import java.time.Duration;
import java.util.Objects;
import java.util.concurrent.ThreadLocalRandom;
import java.util.function.Predicate;

public record RetryConfig(
    int maxAttempts,
    Duration initialDelay,
    Duration maxDelay,
    double backoffMultiplier,
    JitterStrategy jitter,
    Predicate<Throwable> retryPredicate
) {
    public enum JitterStrategy { NONE, FULL, EQUAL, DECORRELATED }

    public RetryConfig {
        if (maxAttempts < 1) throw new IllegalArgumentException("maxAttempts must be >= 1");
        Objects.requireNonNull(initialDelay);
        Objects.requireNonNull(maxDelay);
        Objects.requireNonNull(jitter);
        Objects.requireNonNull(retryPredicate);
    }

    public static RetryConfig defaults() {
        return new RetryConfig(
            3,
            Duration.ofMillis(200),
            Duration.ofSeconds(5),
            2.0,
            JitterStrategy.FULL,
            RetryConfig::isTransient
        );
    }

    public static boolean isTransient(Throwable t) {
        if (t instanceof java.io.IOException) return true;
        if (t instanceof java.util.concurrent.TimeoutException) return true;
        return false;
    }
}
```

---

## 5. Practical Examples

### 5.1 Basic Example: Resilient Retry Engine

```java
import java.time.Duration;
import java.util.concurrent.Callable;
import java.util.concurrent.ThreadLocalRandom;

public final class RetryExecutor {
    private final RetryConfig config;

    public RetryExecutor(RetryConfig config) {
        this.config = config;
    }

    public <T> T execute(Callable<T> task) throws Exception {
        int attempt = 0;
        long currentDelayMs = config.initialDelay().toMillis();
        long maxDelayMs = config.maxDelay().toMillis();

        while (true) {
            attempt++;
            try {
                return task.call();
            } catch (Exception ex) {
                if (attempt >= config.maxAttempts() || !config.retryPredicate().test(ex)) {
                    throw ex; // Terminal failure
                }

                long sleepTimeMs = calculateDelay(attempt, currentDelayMs, maxDelayMs);
                Thread.sleep(Duration.ofMillis(sleepTimeMs));
                currentDelayMs = (long) (currentDelayMs * config.backoffMultiplier());
            }
        }
    }

    private long calculateDelay(int attempt, long currentDelayMs, long maxDelayMs) {
        long ceiling = Math.min(currentDelayMs, maxDelayMs);
        return switch (config.jitter()) {
            case NONE -> ceiling;
            case FULL -> ThreadLocalRandom.current().nextLong(0, ceiling + 1);
            case EQUAL -> {
                long half = ceiling / 2;
                yield half + ThreadLocalRandom.current().nextLong(0, half + 1);
            }
            case DECORRELATED -> {
                long low = config.initialDelay().toMillis();
                yield Math.min(maxDelayMs, ThreadLocalRandom.current().nextLong(low, Math.max(low + 1, currentDelayMs * 3)));
            }
        };
    }
}
```

### 5.2 Real-World Example: HTTP Client with Retry-After Header Compliance

When downstream APIs return `HTTP 429 (Too Many Requests)` or `HTTP 503 (Service Unavailable)`, they often send a `Retry-After: <seconds>` header. Respecting this header overrides local backoff calculations:

```java
import java.net.URI;
import java.net.http.*;
import java.time.Duration;
import java.util.Optional;

public final class ResilientHttpClient {
    private final HttpClient client;
    private final RetryExecutor retryEngine;

    public ResilientHttpClient(HttpClient client, RetryExecutor retryEngine) {
        this.client = client;
        this.retryEngine = retryEngine;
    }

    public HttpResponse<String> sendWithRetry(HttpRequest request) throws Exception {
        return retryEngine.execute(() -> {
            HttpResponse<String> response = client.send(request, HttpResponse.BodyHandlers.ofString());
            int code = response.statusCode();

            if (code == 429 || code == 503) {
                Optional<String> retryAfter = response.headers().firstValue("Retry-After");
                if (retryAfter.isPresent()) {
                    try {
                        long seconds = Long.parseLong(retryAfter.get());
                        Thread.sleep(Duration.ofSeconds(seconds));
                    } catch (NumberFormatException ignored) {}
                }
                throw new java.io.IOException("Transient HTTP status: " + code);
            }

            if (code >= 500) {
                throw new java.io.IOException("Server error HTTP status: " + code);
            }

            return response;
        });
    }
}
```

### 5.3 Edge Case: Deadlock & Interrupted Thread Safety

In modern Java 21+ Virtual Threads, blocking in `Thread.sleep()` yields the carrier thread without blocking operating system resources. However, retry loops must properly handle `InterruptedException`:

```java
public final class ThreadSafeRetryRunner {
    public static <T> T runInterruptibly(Callable<T> task, int maxRetries, long backoffMs) throws Exception {
        int retries = 0;
        while (true) {
            try {
                return task.call();
            } catch (InterruptedException ie) {
                // Restore interrupt status and abort retry immediately
                Thread.currentThread().interrupt();
                throw ie;
            } catch (Exception ex) {
                if (++retries > maxRetries) {
                    throw ex;
                }
                try {
                    Thread.sleep(backoffMs);
                } catch (InterruptedException ie) {
                    Thread.currentThread().interrupt();
                    throw new IllegalStateException("Retry aborted due to thread interruption", ie);
                }
            }
        }
    }
}
```

---

## 6. Best Practices

1. **Only Retry Idempotent Operations**: Never retry non-idempotent mutations (e.g. `POST /payments/charge`) without an idempotency key. A dropped network response after the charge succeeded on the backend causes double-billing.
2. **Always Cap Maximum Delay**: An unbounded exponential backoff ($2^{10} \times 1\text{s} = 1024\text{s}$) leaves threads hanging for 17 minutes. Cap backoff ceiling at 10–30 seconds.
3. **Always Add Jitter**: Never use deterministic backoff in distributed architectures. Full Jitter delivers the lowest client contention and fastest recovery times.
4. **Distinguish Business Errors from System Errors**: HTTP 401 Unauthorized or 422 Validation Error will never succeed on retry; fail fast immediately.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Fix |
| :--- | :--- | :--- |
| **Retrying Non-Idempotent POST** | Causes duplicate financial charges or double-order creation | Ensure `Idempotency-Key` header or retry only GET/PUT/DELETE |
| **Retry Storms** | Thousands of clients synchronize retries, knocking recovering server back down | Apply Full or Decorrelated Jitter |
| **Swallowing InterruptedException** | Prevents graceful container/JVM shutdown | Restore thread interrupt flag: `Thread.currentThread().interrupt()` |
| **Ignoring Circuit Breakers** | When downstream service is completely dead, retrying endlessly wastes CPU/bandwidth | Combine Retry with Circuit Breaker (stop retrying when circuit is OPEN) |
| **Unbounded Max Attempts** | Retrying 100 times exhausts request timeouts and client connection pools | Cap max attempts at 3–5 |

---

## 8. Performance & Security Notes

### 8.1 Virtual Threads vs Platform Threads
- On traditional platform threads (`new Thread()`), 1,000 threads blocked in `Thread.sleep()` during backoff consume ~1 GB of OS stack memory.
- Under Java 21+ Virtual Threads (`Executors.newVirtualThreadPerTaskExecutor()`), `Thread.sleep()` unmounts the virtual thread from its carrier thread. 100,000 virtual threads can sleep concurrently with negligible memory footprint.

### 8.2 Amplification Attacks (Denial of Service)
In nested microservice chains (Service A $\to$ Service B $\to$ Service C), if each service retries 3 times, a single failure at C triggers $3 \times 3 = 9$ retries from B, and $9 \times 3 = 27$ retries from A.
- **Rule**: Retry only at the topmost ingress or the immediate adjacent hop, never at every single layer of a deep call tree.

---

## 9. Comparison / When to Use What

| Strategy | Backoff Predictability | Contention Under Load | Recommended Use Case |
| :--- | :--- | :--- | :--- |
| **Fixed Interval** | Exact | Extreme (Harmonic spikes) | Local hardware poll, test mocks |
| **Exponential Backoff** | Predictable | High | Single client to dedicated service |
| **Full Jitter** | High randomness | Lowest contention | Public cloud APIs, high-throughput microservices |
| **Decorrelated Jitter** | Dynamic | Very Low | Complex distributed systems (AWS recommended) |

---

## 10. Interview Questions

<details>
<summary>1. What is a Retry Storm and how does Jitter prevent it?</summary>
A Retry Storm occurs when a downstream outage causes hundreds of clients to fail and retry at identical deterministic intervals, sending repeated massive waves of requests that keep the recovering server overloaded. Jitter introduces uniform random noise into the backoff delay, spreading retries smoothly across time and allowing the server to recover.
</details>

<details>
<summary>2. Why must you never retry a non-idempotent HTTP POST request without an idempotency key?</summary>
If a network timeout occurs after the server processes the POST request but before the response reaches the client, retrying the POST will execute the mutation a second time, resulting in duplicate charges or duplicate database records.
</details>

<details>
<summary>3. What is the mathematical formula for Full Jitter?</summary>
Full Jitter is defined as $\text{delay} = \text{random}(0, \min(M, B \times 2^n))$, where $B$ is the initial base backoff, $n$ is the attempt index, and $M$ is the ceiling duration.
</details>

<details>
<summary>4. How does Java 21 Virtual Threads affect retry architectures?</summary>
In Java 21+, virtual threads make synchronous backoff (`Thread.sleep()`) virtually cost-free. Instead of complex reactive callback chains, developers can write clean imperative retry loops without exhausting OS thread pools.
</details>

<details>
<summary>5. What is Retry Amplification in a microservice chain?</summary>
When multiple services in a call chain (A -> B -> C) each implement independent retries, the total number of requests multiplies exponentially ($N \times M \times K$), turning a minor blip into an internal denial-of-service attack on the deepest service.
</details>

<details>
<summary>6. How do you handle the `Retry-After` HTTP response header?</summary>
Parse the header value (which can be seconds as an integer or an HTTP-date timestamp) and use that exact duration as the sleep interval before the next attempt, overriding default algorithmic backoff.
</details>

<details>
<summary>7. What exceptions should NEVER be retried?</summary>
Non-transient business exceptions: `IllegalArgumentException`, `NullPointerException`, `AuthenticationException`, `ResourceNotFoundException`, and HTTP 4xx client errors (except 429).
</details>

<details>
<summary>8. How do Circuit Breakers and Retries interact?</summary>
Retries handle transient, short-lived glitches. Circuit Breakers handle long-term outages. Retries should be placed *inside* or *alongside* the circuit breaker: if the circuit breaker trips to OPEN, retries must immediately halt.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Implement an Asynchronous CompletableFuture Retry Wrapper</summary>

```java
import java.time.Duration;
import java.util.concurrent.*;
import java.util.function.Supplier;

public class AsyncRetryer {
    private final ScheduledExecutorService scheduler = Executors.newSingleThreadScheduledExecutor();

    public <T> CompletableFuture<T> retryAsync(Supplier<CompletableFuture<T>> taskSupplier, int retriesLeft, Duration delay) {
        return taskSupplier.get().handle((result, error) -> {
            if (error == null) {
                return CompletableFuture.completedFuture(result);
            }
            if (retriesLeft <= 0) {
                return CompletableFuture.<T>failedFuture(error);
            }
            CompletableFuture<T> next = new CompletableFuture<>();
            scheduler.schedule(() -> {
                retryAsync(taskSupplier, retriesLeft - 1, delay.multipliedBy(2))
                    .whenComplete((res, err) -> {
                        if (err != null) next.completeExceptionally(err);
                        else next.complete(res);
                    });
            }, delay.toMillis(), TimeUnit.MILLISECONDS);
            return next;
        }).thenCompose(f -> f);
    }
}
```
</details>

<details>
<summary>Exercise 2: Implement Decorrelated Jitter Algorithm</summary>

```java
import java.util.concurrent.ThreadLocalRandom;

public class DecorrelatedJitter {
    private final long baseMs;
    private final long maxMs;
    private long lastSleepMs;

    public DecorrelatedJitter(long baseMs, long maxMs) {
        this.baseMs = baseMs;
        this.maxMs = maxMs;
        this.lastSleepMs = baseMs;
    }

    public long nextDelayMs() {
        long sleep = Math.min(maxMs, ThreadLocalRandom.current().nextLong(baseMs, Math.max(baseMs + 1, lastSleepMs * 3)));
        this.lastSleepMs = sleep;
        return sleep;
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Classification**: Only retry transient errors (timeouts, 503, 429, deadlocks). Fail fast on 4xx.
- **Formula**: Exponential backoff $B \times 2^n$ capped at max ceiling $M$.
- **Jitter**: Always apply Full Jitter $\text{rand}(0, \text{backoff})$ to eliminate retry storms.
- **Header**: Respect `Retry-After` if supplied by downstream server.
- **Safety**: Restore `Thread.currentThread().interrupt()` when catching `InterruptedException`.
- **Scope**: Never retry non-idempotent mutations without deduplication / idempotency keys.

---

## 13. Further Reading

- [AWS Architecture Blog: Exponential Backoff And Jitter](https://aws.amazon.com/blogs/architecture/exponential-backoff-and-jitter/)
- [Microsoft Azure Architecture: Retry Pattern](https://learn.microsoft.com/en-us/azure/architecture/patterns/retry)
- [Resilience4j Retry Documentation](https://resilience4j.readme.io/docs/retry)
