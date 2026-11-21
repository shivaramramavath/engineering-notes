# Enterprise Circuit Breaker and Resilience Patterns in Java

> State machine mechanics (CLOSED, OPEN, HALF-OPEN), sliding-window metrics buffers, Bulkhead isolation, graceful fallbacks, and production Java resilience architectures.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [`14-concurrency/01_atomic-and-volatile.md`](../14-concurrency/01_atomic-and-volatile.md), [`25-real-world-patterns/02_retry-and-backoff.md`](02_retry-and-backoff.md) |
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

In distributed microservice architectures, services depend on dozens of downstream HTTP endpoints, databases, and third-party APIs. When a downstream service becomes unresponsive or experiences high latency, incoming caller threads block waiting for socket timeouts.

Within seconds, all caller threads become exhausted waiting on the failing dependency, causing **cascading system failure** (the caller service itself crashes and brings down upstream services).

Inspired by electrical circuit breakers that trip to protect wiring from catastrophic power surges, a software **Circuit Breaker** wraps vulnerable remote calls:
- Detects recurring failures and high latency.
- Trips to **OPEN** immediately, short-circuiting subsequent requests without making network calls.
- Returns instant fallback responses or cached data, preserving system stability and caller responsiveness.
- Periodically probes downstream health (**HALF-OPEN**) to automatically self-heal once the dependency recovers.

---

## 2. Core Concepts

### 2.1 The Three Canonical States

```
                [Normal Operation]
                    +--------+
                    | CLOSED | <---------------------------+
                    +--------+                             |
                        |                                  |
            Failure Rate > Threshold                       |
                        |                                  |
                        v                                  |
    +---------------------------------------+              |
    |                 OPEN                  |              |
    |  (Requests fail fast / return fallback|              |
    +---------------------------------------+              |
                        |                                  |
             Wait Duration In Open State                   |
                        |                        Consecutive Successes
                        v                          >= Permitted
                    +-----------+                          |
                    | HALF-OPEN | -------------------------+
                    +-----------+
                        |
                 Single Failure
                        |
                        v
                     Trips back to OPEN
```

1. **CLOSED**:
   - Requests flow normally to the downstream dependency.
   - Failures and successes are tracked in a sliding window (e.g. last 100 calls).
   - If failure rate exceeds threshold (e.g. 50%), circuit trips to **OPEN**.

2. **OPEN**:
   - Requests fail fast immediately with a `CallNotPermittedException` without network I/O.
   - Configured fallback executes instantly.
   - Remains in OPEN state for a specified duration (e.g. 10–60 seconds).

3. **HALF-OPEN**:
   - Allows a limited trial number of probe requests (e.g. 10 calls) to pass through to the downstream service.
   - If trial calls succeed, circuit transitions back to **CLOSED** (service has recovered).
   - If any trial call fails, circuit immediately returns to **OPEN** for another sleep duration.

### 2.2 Complementary Resilience Patterns: The Bulkhead

Named after watertight compartments in naval ships that prevent a single hull breach from sinking the entire vessel, the **Bulkhead Pattern** isolates resources across distinct operations:
- **Thread Pool Bulkhead**: Dedicated thread pool per dependency (e.g. Pool A for Payment Service, Pool B for Recommendation Engine). Exhausting Pool B cannot starve Pool A.
- **Semaphore Bulkhead**: Bounded concurrent permit limit per dependency without thread pool context-switching overhead.

---

## 3. How It Works Internally

### 3.1 Ring Buffer Sliding Window Mechanics

Modern circuit breakers (such as Resilience4j) calculate failure rates using a fixed-size ring buffer:

```
Ring Buffer of Size 10 (0 = Success, 1 = Failure):
Index:    0    1    2    3    4    5    6    7    8    9
State:  [ 0 ][ 0 ][ 1 ][ 0 ][ 1 ][ 1 ][ 1 ][ 0 ][ 1 ][ 1 ]
Summary: 6 Failures, 4 Successes
Failure Rate: 6 / 10 = 60.0%
If Threshold = 50% -> State transitions atomically to OPEN!
```

---

## 4. Syntax & Usage

### 4.1 Java Record Configuration

```java
import java.time.Duration;
import java.util.Objects;

public record CircuitBreakerConfig(
    int slidingWindowSize,
    int minimumNumberOfCalls,
    double failureRateThreshold, // Percentage (e.g. 50.0)
    Duration waitDurationInOpenState,
    int permittedNumberOfCallsInHalfOpenState
) {
    public CircuitBreakerConfig {
        if (slidingWindowSize <= 0) throw new IllegalArgumentException("slidingWindowSize must be > 0");
        if (minimumNumberOfCalls <= 0) throw new IllegalArgumentException("minimumNumberOfCalls must be > 0");
        if (failureRateThreshold <= 0.0 || failureRateThreshold > 100.0) {
            throw new IllegalArgumentException("failureRateThreshold must be between 0.0 and 100.0");
        }
        Objects.requireNonNull(waitDurationInOpenState);
        if (permittedNumberOfCallsInHalfOpenState <= 0) {
            throw new IllegalArgumentException("permittedNumberOfCallsInHalfOpenState must be > 0");
        }
    }

    public static CircuitBreakerConfig defaults() {
        return new CircuitBreakerConfig(
            100,
            20,
            50.0,
            Duration.ofSeconds(10),
            10
        );
    }
}
```

---

## 5. Practical Examples

### 5.1 Basic Example: Thread-Safe State-Machine Circuit Breaker

```java
import java.time.Duration;
import java.time.Instant;
import java.util.Objects;
import java.util.concurrent.Callable;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

public final class SimpleCircuitBreaker {
    public enum State { CLOSED, OPEN, HALF_OPEN }

    public static class CallNotPermittedException extends RuntimeException {
        public CallNotPermittedException(String message) { super(message); }
    }

    private final CircuitBreakerConfig config;
    private final AtomicReference<State> state = new AtomicReference<>(State.CLOSED);
    private final int[] ringBuffer;
    private int bufferIndex = 0;
    private int totalCallsRecorded = 0;
    private Instant lastTripToOpen = Instant.MIN;
    private final AtomicInteger halfOpenTrialCalls = new AtomicInteger(0);

    public SimpleCircuitBreaker(CircuitBreakerConfig config) {
        this.config = Objects.requireNonNull(config);
        this.ringBuffer = new int[config.slidingWindowSize()];
    }

    public synchronized <T> T execute(Callable<T> task, Callable<T> fallback) throws Exception {
        State current = checkState();

        if (current == State.OPEN) {
            return fallback.call();
        }

        if (current == State.HALF_OPEN) {
            if (halfOpenTrialCalls.incrementAndGet() > config.permittedNumberOfCallsInHalfOpenState()) {
                return fallback.call();
            }
        }

        try {
            T result = task.call();
            recordSuccess();
            return result;
        } catch (Exception ex) {
            recordFailure();
            return fallback.call();
        }
    }

    private State checkState() {
        State s = state.get();
        if (s == State.OPEN) {
            if (Instant.now().isAfter(lastTripToOpen.plus(config.waitDurationInOpenState()))) {
                state.set(State.HALF_OPEN);
                halfOpenTrialCalls.set(0);
                return State.HALF_OPEN;
            }
        }
        return s;
    }

    private synchronized void recordSuccess() {
        if (state.get() == State.HALF_OPEN) {
            if (halfOpenTrialCalls.get() >= config.permittedNumberOfCallsInHalfOpenState()) {
                transitionToClosed();
            }
            return;
        }

        ringBuffer[bufferIndex] = 0;
        advanceBuffer();
    }

    private synchronized void recordFailure() {
        if (state.get() == State.HALF_OPEN) {
            transitionToOpen();
            return;
        }

        ringBuffer[bufferIndex] = 1;
        advanceBuffer();

        if (totalCallsRecorded >= config.minimumNumberOfCalls()) {
            int failures = 0;
            int count = Math.min(totalCallsRecorded, config.slidingWindowSize());
            for (int i = 0; i < count; i++) {
                failures += ringBuffer[i];
            }
            double rate = ((double) failures / count) * 100.0;
            if (rate >= config.failureRateThreshold()) {
                transitionToOpen();
            }
        }
    }

    private void advanceBuffer() {
        bufferIndex = (bufferIndex + 1) % config.slidingWindowSize();
        if (totalCallsRecorded < config.slidingWindowSize()) {
            totalCallsRecorded++;
        }
    }

    private void transitionToOpen() {
        state.set(State.OPEN);
        lastTripToOpen = Instant.now();
        System.err.println("Circuit Breaker TRIPPED to OPEN!");
    }

    private void transitionToClosed() {
        state.set(State.CLOSED);
        bufferIndex = 0;
        totalCallsRecorded = 0;
        System.out.println("Circuit Breaker RECOVERED to CLOSED.");
    }

    public State getState() { return state.get(); }
}
```

### 5.2 Real-World Example: Semaphore Bulkhead with Degraded Fallback

```java
import java.util.concurrent.Callable;
import java.util.concurrent.Semaphore;

public final class SemaphoreBulkhead {
    private final Semaphore semaphore;
    private final String name;

    public SemaphoreBulkhead(String name, int maxConcurrentCalls) {
        this.name = name;
        this.semaphore = new Semaphore(maxConcurrentCalls);
    }

    public <T> T execute(Callable<T> task, Callable<T> fallback) throws Exception {
        if (!semaphore.tryAcquire()) {
            System.err.println("Bulkhead '" + name + "' full. Rejecting call and executing fallback.");
            return fallback.call();
        }

        try {
            return task.call();
        } finally {
            semaphore.release();
        }
    }
}
```

### 5.3 Edge Case: Differentiating Network Outages from Business Validation

Not all exceptions indicate that the remote dependency is broken:
- Network timeouts (`SocketTimeoutException`, `ConnectException`): Infrastructure failures $\to$ Record against Circuit Breaker.
- Business validation (`HTTP 400 Bad Request`, `IllegalArgumentException`): Client errors $\to$ Do NOT trip the Circuit Breaker.

```java
public class SelectiveExceptionClassifier {
    public static boolean isSystemFailure(Throwable t) {
        if (t instanceof java.io.IOException) return true;
        if (t instanceof java.util.concurrent.TimeoutException) return true;
        return false;
    }
}
```

---

## 6. Best Practices

1. **Always Provide a Meaningful Fallback**: Fallbacks should return cached data, stubbed responses, or gracefully degraded UI (e.g. "Product reviews temporarily unavailable" rather than a white error screen).
2. **Set `minimumNumberOfCalls` Appropriately**: Never trip a circuit on 2 failures out of 2 requests on cold start. Mandate a statistical sample size (e.g. minimum 20 or 50 calls) before calculating failure rate.
3. **Combine with Retries Correctly**: Place Retry *inside* Circuit Breaker. If the service is OPEN, retries do not execute.
4. **Export Metrics to Prometheus / Grafana**: Alert when a circuit enters OPEN state in production.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Fix |
| :--- | :--- | :--- |
| **Tripping on Client Errors (4xx)** | Malicious client sending invalid requests trips circuit for all users | Filter out HTTP 4xx errors from failure counts |
| **Sleep Window Too Short** | 100ms wait in OPEN state overwhelms the struggling server before it can reboot | Set open duration to 10–60 seconds |
| **Missing Minimum Sample Size** | 1 failed request out of 1 request yields 100% failure rate, immediately tripping circuit | Enforce `minimumNumberOfCalls >= 20` |
| **Blocking Fallbacks** | Fallback method itself makes an un-protected database call that also hangs | Keep fallbacks strictly local, non-blocking, and in-memory |
| **Unbounded Thread Pools** | Running a thread-pool bulkhead with an unbounded queue queues 100,000 requests, causing latency spikes | Use bounded queues and reject fast |

---

## 8. Performance & Security Notes

### 8.1 Performance Overhead
- Semaphore Bulkhead: Negligible overhead (atomic decrement/increment).
- Thread Pool Bulkhead: Context switching and thread scheduling overhead. Prefer Semaphore Bulkhead unless task isolation requires separate CPU quotas.

### 8.2 Security: Fallback Data Leakage
Ensure that stubbed fallback responses do not expose sensitive internal debug strings or bypass security authorization layers.

---

## 9. Comparison / When to Use What

| Pattern | Purpose | Action When Overloaded |
| :--- | :--- | :--- |
| **Circuit Breaker** | Prevent cascading failure from failing dependency | Short-circuits calls to OPEN state; executes fallback |
| **Rate Limiter** | Protect server from abusive callers | Returns HTTP 429 Too Many Requests |
| **Bulkhead** | Isolate system resources across domains | Drops excess requests to preserve other modules |
| **Retry** | Overcome transient network glitches | Retries request with backoff and jitter |
| **Timeout** | Prevent threads from waiting indefinitely | Cancels request after deadline expires |

---

## 10. Interview Questions

<details>
<summary>1. Describe the three states of a Circuit Breaker and their transitions.</summary>
- **CLOSED**: Normal state. Remote calls execute. If failure rate exceeds threshold, trips to OPEN.
- **OPEN**: Remote calls fail fast without network execution. After a timeout duration, transitions to HALF-OPEN.
- **HALF-OPEN**: Allows a limited trial number of probe calls. If they succeed, transitions to CLOSED. If any fail, returns to OPEN.
</details>

<details>
<summary>2. Why should you NOT count HTTP 4xx client errors as circuit breaker failures?</summary>
HTTP 4xx errors indicate client-side mistakes (e.g. invalid syntax, unauthorized credentials, or missing resources). The remote dependency is running normally. Counting 4xx errors would allow an abusive or broken client to trip the circuit breaker and disable service for all other legitimate users.
</details>

<details>
<summary>3. What is the Bulkhead pattern and how does it prevent cascading failures?</summary>
The Bulkhead pattern isolates system resources (e.g. thread pools or connection limits) into separate pools for each downstream dependency. If one dependency stalls, only its dedicated pool is exhausted, allowing the rest of the application to operate unimpeded.
</details>

<details>
<summary>4. How does Resilience4j differ from Netflix Hystrix?</summary>
Netflix Hystrix is deprecated and relied heavily on separate OS thread pools per command, incurring high context-switching overhead. Resilience4j is a modern, lightweight, functional Java library designed for Java 8+ that uses Semaphore isolation and lock-free atomic ring buffers by default.
</details>

<details>
<summary>5. What is the difference between count-based and time-based sliding windows?</summary>
- **Count-based**: Evaluates failure rate over the last $N$ calls (e.g. last 100 requests).
- **Time-based**: Evaluates failure rate over the last $T$ seconds (e.g. last 60 seconds).
</details>

<details>
<summary>6. Where should the Retry pattern be placed relative to the Circuit Breaker?</summary>
Retries should be placed *inside* the Circuit Breaker. If a request fails, the retry loop handles transient blips. If all retries fail, a single failure is recorded in the Circuit Breaker. If the Circuit Breaker trips to OPEN, incoming requests fail fast immediately without triggering retries.
</details>

<details>
<summary>7. What is a "slow call rate threshold"?</summary>
A metric where requests that succeed but take longer than a specified duration threshold (e.g. $> 2$ seconds) are classified as slow calls. If the percentage of slow calls exceeds a threshold (e.g. 50%), the circuit breaker trips to OPEN, protecting the system from thread exhaustion before actual timeouts occur.
</details>

<details>
<summary>8. What are common fallback strategies in production systems?</summary>
1. Returning stale cached data.
2. Returning a default/stubbed response (e.g. empty recommendations list).
3. Queueing the request for asynchronous processing.
4. Returning a graceful degradation response indicating temporary unavailability.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Implement a Time-Windowed Circuit Breaker State Transition</summary>

```java
import java.time.Duration;
import java.time.Instant;

public class CircuitHealthProbe {
    private Instant openTimestamp = Instant.MIN;
    private final Duration recoveryWait = Duration.ofSeconds(30);

    public void trip() {
        this.openTimestamp = Instant.now();
    }

    public boolean canProbe() {
        return Instant.now().isAfter(openTimestamp.plus(recoveryWait));
    }
}
```
</details>

<details>
<summary>Exercise 2: Implement a Resilient Fallback Decorator</summary>

```java
import java.util.function.Supplier;

public class FallbackDecorator {
    public static <T> T executeWithFallback(Supplier<T> primary, Supplier<T> fallback) {
        try {
            return primary.get();
        } catch (Exception e) {
            System.err.println("Primary failed (" + e.getMessage() + "). Invoking fallback...");
            return fallback.get();
        }
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **CLOSED**: Calls allowed $\to$ Trips to OPEN if failures exceed threshold.
- **OPEN**: Fast-fail / execute fallback $\to$ Waits duration $\to$ Enters HALF-OPEN.
- **HALF-OPEN**: Probes downstream service with limited requests $\to$ Recovers to CLOSED on success.
- **Bulkhead**: Limits concurrent execution per dependency using Semaphores or Thread Pools.
- **Filter**: Trip only on infrastructure failures (timeouts, 503), never on business validation errors (4xx).
- **Fallbacks**: Must be fast, safe, and non-blocking (e.g. cache or empty stubs).

---

## 13. Further Reading

- [Michael Nygard, *Release It! Design and Deploy Production-Ready Software*](https://pragprog.com/titles/mnee2/release-it-second-edition/)
- [Martin Fowler: Circuit Breaker Architecture](https://martinfowler.com/bliki/CircuitBreaker.html)
- [Resilience4j Official Documentation](https://resilience4j.readme.io/)
