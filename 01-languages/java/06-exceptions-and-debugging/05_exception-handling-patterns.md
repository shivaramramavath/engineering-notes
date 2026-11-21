# Exception Handling Patterns and Anti-Patterns in Java

> An authoritative guide to resilient exception architecture, fault tolerance patterns (Retry, Circuit Breaker, Fallback), and lethal anti-patterns to avoid.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate to Advanced |
| **Prerequisites** | [00_exception-hierarchy-and-types.md](./00_exception-hierarchy-and-types.md), [01_try-catch-finally.md](./01_try-catch-finally.md) |
| **Time to Read** | 25 minutes |
| **Status** | Active |
| **Java Version** | Java 8+ (Java 25 LTS verified) |

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
9. [Comparison: Fault Tolerance Patterns](#9-comparison-fault-tolerance-patterns)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

In distributed systems and enterprise architectures, failure is not an anomaly; it is an everyday certainty. Networks partition, third-party APIs experience rate limiting, database connections pool out, and disk storage saturates.

How an application structures its exception handling defines its resilience:
- **Fragile systems** crash entirely on transient network blips, leak database connections, and produce misleading log files.
- **Resilient systems** isolate failures, gracefully degrade non-critical functionality, retry transient operations with exponential backoff, and circuit-break upstream dependencies before cascading failures bring down the cluster.

---

## 2. Core Concepts

### 2.1 The Hall of Shame: Top Exception Anti-Patterns
1. **Catch-and-Ignore (The Black Hole):** `catch (Exception e) {}` completely erases failures without trace.
2. **Log-and-Rethrow:** Logging an exception and immediately re-throwing it floods logs with duplicate stack traces.
3. **Catching Generic `Exception` or `Throwable`:** Masks genuine programming bugs (NPEs, index errors) as transient business failures.
4. **Destructive Wrapping:** Catching an exception and re-throwing a new one without passing the original as the `cause`.
5. **Exceptions for Control Flow:** Using `try-catch` to exit loops or validate ordinary user input.

### 2.2 Core Fault Tolerance Patterns
- **Retry Pattern:** Retries idempotent transient operations with exponential backoff and jitter.
- **Circuit Breaker Pattern:** Detects recurring failures and halts calls to failing systems immediately to prevent resource exhaustion.
- **Fallback Pattern:** Returns cached or default degraded responses when primary dependencies fail.

---

## 3. How It Works Internally

### 3.1 Retry with Exponential Backoff and Full Jitter
When retrying distributed operations, naive immediate retries create a "thundering herd" that overwhelms recovering services.
The mathematical formula for **Exponential Backoff with Full Jitter** is:
$$\text{Sleep Time} = \text{random}(0, \min(M, B \times 2^{\text{attempt}}))$$
Where $B$ is base backoff (e.g. 100ms) and $M$ is maximum ceiling backoff (e.g. 5,000ms). Jitter distributes retry bursts uniformly over time.

---

## 4. Syntax & Usage

### 4.1 Implementing Resilient Retries
```java
public class RetryExecutor {
    public static <T> T executeWithRetry(Supplier<T> task, int maxRetries, long baseBackoffMs) {
        int attempt = 0;
        while (true) {
            try {
                return task.get();
            } catch (TransientNetworkException ex) {
                attempt++;
                if (attempt > maxRetries) throw ex;
                long sleepMs = (long) (baseBackoffMs * Math.pow(2, attempt - 1));
                try { Thread.sleep(sleepMs); } catch (InterruptedException ie) {
                    Thread.currentThread().interrupt();
                    throw new RuntimeException("Retry interrupted", ie);
                }
            }
        }
    }
}
```

---

## 5. Practical Examples

### Example 1: Resilient Retry with Exponential Backoff
```java
import java.util.concurrent.ThreadLocalRandom;
import java.util.function.Supplier;

public class ResilientRetryDemo {
    public static <T> T retry(Supplier<T> operation, int maxAttempts, long baseDelayMs) throws Exception {
        int attempt = 1;
        while (true) {
            try {
                return operation.get();
            } catch (Exception ex) {
                if (attempt >= maxAttempts) {
                    System.out.println("Exhausted all " + maxAttempts + " attempts. Failing fast.");
                    throw ex;
                }
                // Exponential backoff with jitter
                long maxSleep = (long) (baseDelayMs * Math.pow(2, attempt));
                long sleepTime = ThreadLocalRandom.current().nextLong(10, maxSleep);

                System.out.printf("Attempt %d failed: %s. Retrying in %d ms...%n",
                    attempt, ex.getMessage(), sleepTime);

                Thread.sleep(sleepTime);
                attempt++;
            }
        }
    }

    public static void main(String[] args) throws Exception {
        int[] counter = {0};
        String result = retry(() -> {
            counter[0]++;
            if (counter[0] < 3) throw new RuntimeException("Network timeout (504)");
            return "SUCCESSFUL_PAYLOAD";
        }, 5, 50);

        System.out.println("Final Result: " + result);
    }
}
```
**Output:**
```text
Attempt 1 failed: Network timeout (504). Retrying in ... ms...
Attempt 2 failed: Network timeout (504). Retrying in ... ms...
Final Result: SUCCESSFUL_PAYLOAD
```

### Example 2: Minimalist Circuit Breaker State Machine
```java
import java.util.function.Supplier;

public class SimpleCircuitBreaker {
    public enum State { CLOSED, OPEN, HALF_OPEN }

    private State state = State.CLOSED;
    private int failureCount = 0;
    private final int failureThreshold;
    private final long openDurationMs;
    private long lastStateChangeTime = 0;

    public SimpleCircuitBreaker(int threshold, long durationMs) {
        this.failureThreshold = threshold;
        this.openDurationMs = durationMs;
    }

    public synchronized <T> T execute(Supplier<T> operation, Supplier<T> fallback) {
        long now = System.currentTimeMillis();

        if (state == State.OPEN) {
            if (now - lastStateChangeTime > openDurationMs) {
                state = State.HALF_OPEN;
                System.out.println("[CIRCUIT] State changed to HALF_OPEN. Testing upstream...");
            } else {
                System.out.println("[CIRCUIT] Circuit is OPEN! Short-circuiting call to fallback.");
                return fallback.get();
            }
        }

        try {
            T result = operation.get();
            if (state == State.HALF_OPEN) {
                state = State.CLOSED;
                failureCount = 0;
                System.out.println("[CIRCUIT] Upstream healthy! State reset to CLOSED.");
            }
            return result;
        } catch (Exception ex) {
            failureCount++;
            if (failureCount >= failureThreshold) {
                state = State.OPEN;
                lastStateChangeTime = now;
                System.out.println("[CIRCUIT] Threshold reached! State changed to OPEN.");
            }
            return fallback.get();
        }
    }

    public static void main(String[] args) {
        SimpleCircuitBreaker cb = new SimpleCircuitBreaker(2, 500);

        for (int i = 0; i < 4; i++) {
            String res = cb.execute(
                () -> { throw new RuntimeException("DB Connection Timeout"); },
                () -> "FALLBACK_CACHED_DATA"
            );
            System.out.println("Invocation " + (i + 1) + " returned: " + res);
        }
    }
}
```
**Output:**
```text
Invocation 1 returned: FALLBACK_CACHED_DATA
[CIRCUIT] Threshold reached! State changed to OPEN.
Invocation 2 returned: FALLBACK_CACHED_DATA
[CIRCUIT] Circuit is OPEN! Short-circuiting call to fallback.
Invocation 3 returned: FALLBACK_CACHED_DATA
[CIRCUIT] Circuit is OPEN! Short-circuiting call to fallback.
Invocation 4 returned: FALLBACK_CACHED_DATA
```

### Example 3: Edge Case – Preserving Interrupted Status on `InterruptedException`
```java
public class InterruptionHandlingDemo {
    public static void backgroundWorker() {
        try {
            Thread.sleep(5000);
        } catch (InterruptedException ex) {
            // CRITICAL REQUIREMENT: InterruptedException clears the thread's interrupted flag!
            // If we don't restore it, higher-level orchestrators will not know the thread was cancelled!
            Thread.currentThread().interrupt(); // Restore interrupted status!
            System.out.println("Restored thread interrupted status safely.");
        }
    }

    public static void main(String[] args) {
        Thread t = new Thread(InterruptionHandlingDemo::backgroundWorker);
        t.start();
        t.interrupt();
    }
}
```
**Output:**
```text
Restored thread interrupted status safely.
```

### Example 4: Failure Case – Catch-and-Ignore Disaster
```java
public class BlackHoleAntiPattern {
    public static void saveUserSettings(String userId) {
        try {
            // Database writes fail due to disk full...
            throw new java.io.IOException("No space left on device");
        } catch (Exception e) {
            // LETHAL FLAW: Empty catch block hides silent data loss!
            // Application reports "Settings saved successfully!" to user!
        }
    }

    public static void main(String[] args) {
        saveUserSettings("USR-101");
        System.out.println("Settings reported saved, but database write silently failed!");
    }
}
```
**Output:**
```text
Settings reported saved, but database write silently failed!
```

### Example 5: Optimized / Idiomatic – Centralized Fail-Fast Architecture
```java
public class CentralizedExceptionRouter {
    public record ApiResponse<T>(boolean success, T data, String errorCode, String errorDetail) {}

    public static <T> ApiResponse<T> executeApi(Supplier<T> endpoint) {
        try {
            return new ApiResponse<>(true, endpoint.get(), null, null);
        } catch (IllegalArgumentException ex) {
            return new ApiResponse<>(false, null, "INVALID_ARGUMENT", ex.getMessage());
        } catch (IllegalStateException ex) {
            return new ApiResponse<>(false, null, "BAD_STATE", ex.getMessage());
        } catch (Exception ex) {
            // General catch-all logs full trace and returns generic correlation ID
            String errorId = "ERR-" + System.currentTimeMillis();
            System.err.println("Fatal unexpected error [" + errorId + "]: " + ex.getMessage());
            return new ApiResponse<>(false, null, "INTERNAL_ERROR", "Contact support with ID: " + errorId);
        }
    }

    public static void main(String[] args) {
        var res1 = executeApi(() -> "Hello World");
        var res2 = executeApi(() -> { throw new IllegalArgumentException("Missing token"); });
        System.out.println("Res 1: " + res1);
        System.out.println("Res 2: " + res2);
    }
}
```
**Output:**
```text
Res 1: ApiResponse[success=true, data=Hello World, errorCode=null, errorDetail=null]
Res 2: ApiResponse[success=false, data=null, errorCode=INVALID_ARGUMENT, errorDetail=Missing token]
```

---

## 6. Best Practices

1. **Always Restore Interrupted Status:** Whenever catching `InterruptedException`, always re-assert the flag using `Thread.currentThread().interrupt()`.
2. **Never Swallow Exceptions in Empty Catch Blocks:** At minimum, log the exception at `WARN` or `ERROR` level, or rethrow as a domain exception.
3. **Log OR Rethrow, Never Both:** Either log the failure and recover, OR throw the failure to caller layers. Never duplicate log entries.
4. **Use Jitter with Exponential Backoff:** Avoid strict interval retries to prevent synchronized thundering herds against failing downstream dependencies.
5. **Circuit-Break External Dependencies:** Wrap third-party REST and database clients with circuit breakers to fail fast when services degrade.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Is Wrong | Correct Idiom |
| :--- | :--- | :--- |
| Catch and ignore (`catch (e) {}`) | Silently hides catastrophic data loss and production corruption. | Log error or rethrow. |
| Catching `InterruptedException` without restoring flag | Erases cancellation signal; thread pools cannot gracefully terminate. | `Thread.currentThread().interrupt();`. |
| Unbounded retries | Retries indefinitely, causing thread starvation and blocking request queues. | Bound retries (e.g. 3 attempts max). |
| Catching `Exception` in business logic | Catches unexpected bugs (`NullPointerException`) that should have failed fast. | Catch specific anticipated exceptions. |
| Returning `null` on error | Forces all downstream callers to add repetitive null checks or face NPE. | Return `Optional`, throw exception, or return fallback. |

---

## 8. Performance & Security Notes

### 8.1 Thundering Herd Problem
When an upstream cache or database drops offline, hundreds of worker threads may simultaneously encounter exceptions and trigger retries. Without randomized jitter (`ThreadLocalRandom`), all threads sleep for identical durations and retry simultaneously, generating cyclical load spikes that keep the database from recovering.

---

## 9. Comparison: Fault Tolerance Patterns

| Pattern | Goal | When to Apply | Risk if Omitted |
| :--- | :--- | :--- | :--- |
| **Retry** | Overcome transient glitches | Idempotent network calls | Permanent failure on 1ms glitch |
| **Circuit Breaker** | Prevent resource exhaustion | Repeated third-party outages | Thread pool starvation & outage |
| **Fallback** | Degraded user experience | Non-critical UI elements | Complete application blackout |
| **Fail-Fast** | Stop processing bad state | Invalid inputs, corrupted DB | Data corruption in storage |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why must you always call Thread.currentThread().interrupt() when catching InterruptedException?</strong></summary>

Catching `InterruptedException` automatically clears the thread's interrupted status flag. If you catch the exception and do not rethrow it, higher-level orchestrators (such as `ExecutorService`, Spring, or application shutdown hooks) have no way of knowing that the thread received a cancellation signal. Restoring the flag via `Thread.currentThread().interrupt()` preserves the cancellation contract.
</details>

<details>
<summary><strong>2. What is the Circuit Breaker pattern and how does its state machine work?</strong></summary>

The Circuit Breaker pattern protects systems from cascading failures by failing fast when downstream services are unhealthy:
- **CLOSED:** Normal state. Calls pass through. If failures exceed a threshold, circuit transitions to OPEN.
- **OPEN:** Calls fail fast immediately without contacting downstream service, routing directly to fallbacks.
- **HALF-OPEN:** After a timeout, trial requests are allowed through. If successful, state resets to CLOSED; if they fail, state reverts to OPEN.
</details>

<details>
<summary><strong>3. What is the "Thundering Herd" problem in exception retry architecture?</strong></summary>

It occurs when multiple concurrent clients experience the same service failure and retry after identical backoff delays. When the delay elapses, all clients hit the recovering service simultaneously, triggering another overload spike and failure. Adding randomized "jitter" spreads retry traffic evenly across time.
</details>

<details>
<summary><strong>4. What is the danger of catching java.lang.Exception in a controller or service method?</strong></summary>

It intercepts not only intended checked exceptions, but also unchecked programming bugs (e.g. `NullPointerException`, `IndexOutOfBoundsException`, `ClassCastException`). Instead of failing fast and alerting developers to a software bug, the code attempts to treat the programming bug as an expected environmental failure.
</details>

<details>
<summary><strong>5. Why is using exceptions for normal loop termination an anti-pattern?</strong></summary>

Because exceptions are designed for exceptional events. Generating an exception allocates heap memory, populates stack frames via `fillInStackTrace()`, and bypasses CPU branch prediction, degrading performance by orders of magnitude compared to an ordinary `for` or `while` loop condition.
</details>

<details>
<summary><strong>6. What is the Fallback pattern?</strong></summary>

The Fallback pattern provides an alternative response when a primary operation fails. Examples include returning cached read data, returning an empty list of recommendations, or queuing an order for asynchronous retry rather than displaying an error page.
</details>

<details>
<summary><strong>7. How should exceptions be logged in production?</strong></summary>

Log the exception once with a structured logger (`logger.error("Failed to process order {}: {}", orderId, ex.getMessage(), ex)`). The logger captures the complete stack trace and message. Never log and rethrow, which floods log aggregators with duplicate lines.
</details>

<details>
<summary><strong>8. What is the difference between Fail-Fast and Graceful Degradation?</strong></summary>

- **Fail-Fast:** Immediately aborts an operation at the first sign of invalid data or unexpected failure (e.g. bad configuration, invalid arguments), preventing corrupted data from persisting.
- **Graceful Degradation:** Sacrifices non-essential features (e.g. disabling live user avatars or real-time analytics) while keeping the core application functioning during partial outages.
</details>

---

## 11. Practice Exercises

### Easy
1. **Restore Interruption:** Write a method that sleeps for 1 second, catches `InterruptedException`, and restores the interrupted status.
2. **Safe Parser Fallback:** Implement a method `int parseIntOrDefault(String s, int defaultVal)` using try-catch cleanly.
3. **Duplicate Log Detector:** Demonstrate why logging and rethrowing produces duplicate log lines.

### Medium
4. **Exponential Backoff Retrier:** Implement a generic `RetryUtils.execute()` method with max attempts, backoff multiplier, and jitter.
5. **Fault-Tolerant Cache Service:** Build a service that tries fetching from a remote API and falls back to a local in-memory cache on `TimeoutException`.
6. **Thread Interrupt Propagation:** Build a worker task running inside an `ExecutorService` that terminates cleanly when interrupted.

### Hard
7. **Production-Ready Circuit Breaker:** Implement a complete thread-safe Circuit Breaker with `AtomicInteger` failure counts and state transitions.
8. **Asynchronous Retry with CompletableFuture:** Implement non-blocking asynchronous retries using `CompletableFuture` and scheduled executors.
9. **Dead-Letter Queue Processor:** Build a pipeline that routes failed records to a Dead-Letter Queue (DLQ) after 3 failed retries.

<details>
<summary><strong>Exercise Solutions</strong></summary>

```java
// Solution to Exercise 1: Restore Interruption
public class InterruptHelper {
    public static void safeSleep(long millis) {
        try {
            Thread.sleep(millis);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt(); // Restore flag
        }
    }
}
```
</details>

---

## 12. Summary & Cheat Sheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                   EXCEPTION PATTERNS CHEAT SHEET                       │
├────────────────────────────────────────────────────────────────────────┤
│ Anti-Pattern 1:     Catch-and-ignore (Swallows errors silently)        │
│ Anti-Pattern 2:     Log-and-rethrow (Duplicates stack traces in logs)  │
│ Anti-Pattern 3:     Exceptions for flow control (Slow and unreadable)  │
│ InterruptedException:ALWAYS call Thread.currentThread().interrupt()    │
│ Retry Pattern:      Use Exponential Backoff + Jitter for transient errs│
│ Circuit Breaker:    CLOSED -> OPEN -> HALF_OPEN (Prevents cascade fail)│
│ Fallback:           Return cached or default data during outages       │
│ Principle:          Fail fast on bugs; degrade gracefully on transient │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading
- *Effective Java (3rd Edition)*, Joshua Bloch, Item 69: "Use exceptions only for exceptional conditions".
- *Release It! (2nd Edition)*, Michael Nygard: Stability Patterns (Circuit Breaker, Timeout, Fallback).
- AWS Architecture Blog: "Exponential Backoff And Jitter".
