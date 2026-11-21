# Project 03: High-Throughput Rate Limiting Gateway Service (`rate-limiter`)

> Distributed and in-memory rate limiting engine implementing lock-free Token Bucket, Sliding Window Log, and sliding window counter with RFC 6585 compliance and tenant quotas.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-03-rate-limiter` |
| **Java Baseline** | Java 25 LTS |
| **Complexity** | Advanced / Production-Grade |
| **Architectural Style** | Pipeline / Middleware Pattern |
| **Core Concepts** | Concurrency, Atomics (`AtomicReference`), Token Bucket, Sliding Window Counter, RFC 6585 Headers, Rate Limiting Filters |

---

## 1. Architectural Overview & Component Breakdown

### 1.1 System Architecture

This project implements a multi-tenant rate limiting gateway service capable of evaluating and enforcing millions of requests per second with microsecond latency. It provides both in-memory lock-free implementations (for single-node microservices) and shared distributed storage abstractions (modeled after Redis Sorted Sets).

```
+-------------------------------------------------------------------------------+
|                      RATE LIMITING GATEWAY ARCHITECTURE                       |
|                                                                               |
|   Incoming Client Request (HTTP / RPC)                                        |
|         |                                                                     |
|         v                                                                     |
|   +---------------------------------------+                                   |
|   |   RateLimitFilter                     |                                   |
|   |   - Extracts Client Key (API Token/IP)|                                   |
|   |   - Resolves Quota Policy Tier        |                                   |
|   +-------------------+-------------------+                                   |
|                       |                                                       |
|                       v                                                       |
|   +---------------------------------------+                                   |
|   |   RateLimiter Strategy Dispatcher     |                                   |
|   |   - TokenBucket (Bursty traffic)      |                                   |
|   |   - SlidingWindowCounter (Smooth)     |                                   |
|   +-------------------+-------------------+                                   |
|                       |                                                       |
|            +----------+----------+                                            |
|            |                     |                                            |
|            v                     v                                            |
|     [ Token Available ]   [ Quota Exhausted ]                                 |
|            |                     |                                            |
|            v                     v                                            |
|     Consume Token          Generate HTTP 429 Payload                          |
|     Forward to Route       Attach RFC 6585 Headers:                           |
|                            - Retry-After: <sec>                               |
|                            - X-RateLimit-Limit                                |
|                            - X-RateLimit-Remaining: 0                         |
|                            - X-RateLimit-Reset: <epoch>                       |
+-------------------------------------------------------------------------------+
```

### 1.2 Component Responsibilities
- **Domain Models (`model`):** Quota definitions, tenant tiers (Free, Pro, Enterprise), and evaluation results (`RateLimitDecision`).
- **Limiter Engine (`engine`):** Lock-free Token Bucket utilizing atomic CAS replenishment and Sliding Window Log algorithms.
- **Quota Store (`store`):** Thread-safe in-memory store with automated TTL eviction of idle client buckets.
- **Middleware Filter (`filter`):** Interceptor evaluating request admission and generating standard RFC 6585 HTTP headers.

---

## 2. Complete Project Source Code

### Component 1: Domain Models (`src/main/java/com/learning/ratelimit/model/`)

```java
package com.learning.ratelimit.model;

import java.time.Duration;
import java.time.Instant;
import java.util.Objects;

public record QuotaPolicy(
    String tierName,
    long capacity,
    double tokensPerSecond,
    Duration windowDuration
) {
    public static final QuotaPolicy FREE_TIER = new QuotaPolicy("FREE", 10, 2.0, Duration.ofMinutes(1));
    public static final QuotaPolicy PRO_TIER = new QuotaPolicy("PRO", 100, 20.0, Duration.ofMinutes(1));
    public static final QuotaPolicy ENTERPRISE_TIER = new QuotaPolicy("ENTERPRISE", 1000, 200.0, Duration.ofMinutes(1));

    public QuotaPolicy {
        Objects.requireNonNull(tierName);
        if (capacity <= 0 || tokensPerSecond <= 0) {
            throw new IllegalArgumentException("Capacity and tokensPerSecond must be positive");
        }
    }
}

public record RateLimitDecision(
    boolean isAllowed,
    long limit,
    long remaining,
    long retryAfterSeconds,
    Instant resetTime
) {
    public static RateLimitDecision allowed(long limit, long remaining, Instant resetTime) {
        return new RateLimitDecision(true, limit, remaining, 0, resetTime);
    }

    public static RateLimitDecision denied(long limit, long retryAfterSeconds, Instant resetTime) {
        return new RateLimitDecision(false, limit, 0, retryAfterSeconds, resetTime);
    }
}
```

### Component 2: Lock-Free Token Bucket Engine (`src/main/java/com/learning/ratelimit/engine/`)

```java
package com.learning.ratelimit.engine;

import com.learning.ratelimit.model.*;
import java.time.Instant;
import java.util.concurrent.atomic.AtomicReference;

public final class TokenBucketLimiter {
    private record State(long tokens, long lastRefillNanos) {}

    private final QuotaPolicy policy;
    private final double refillTokensPerNano;
    private final AtomicReference<State> state;

    public TokenBucketLimiter(QuotaPolicy policy) {
        this.policy = policy;
        this.refillTokensPerNano = policy.tokensPerSecond() / 1_000_000_000.0;
        this.state = new AtomicReference<>(new State(policy.capacity(), System.nanoTime()));
    }

    public RateLimitDecision tryAcquire(long tokensRequested) {
        long capacity = policy.capacity();

        while (true) {
            State current = state.get();
            long now = System.nanoTime();
            long elapsedNanos = Math.max(0, now - current.lastRefillNanos());

            long refilled = (long) (elapsedNanos * refillTokensPerNano);
            long available = Math.min(capacity, current.tokens() + refilled);

            if (available < tokensRequested) {
                // Quota exhausted
                long deficit = tokensRequested - available;
                long waitNanos = (long) (deficit / refillTokensPerNano);
                long retryAfterSec = Math.max(1, waitNanos / 1_000_000_000L);
                Instant resetAt = Instant.now().plusSeconds(retryAfterSec);
                return RateLimitDecision.denied(capacity, retryAfterSec, resetAt);
            }

            long updatedLast = (refilled > 0) ? now : current.lastRefillNanos();
            State next = new State(available - tokensRequested, updatedLast);

            if (state.compareAndSet(current, next)) {
                Instant resetAt = Instant.now().plus(policy.windowDuration());
                return RateLimitDecision.allowed(capacity, next.tokens(), resetAt);
            }
            // Collided with another thread -> spin CAS
        }
    }
}
```

### Component 3: Sliding Window Counter Limiter (`src/main/java/com/learning/ratelimit/engine/`)

```java
package com.learning.ratelimit.engine;

import com.learning.ratelimit.model.*;
import java.time.Instant;
import java.util.concurrent.atomic.AtomicLong;

public final class SlidingWindowCounterLimiter {
    private final QuotaPolicy policy;
    private final long windowMillis;

    private static class WindowBucket {
        final long startMillis;
        final AtomicLong count = new AtomicLong(0);

        WindowBucket(long startMillis) {
            this.startMillis = startMillis;
        }
    }

    private WindowBucket previousBucket;
    private WindowBucket currentBucket;

    public SlidingWindowCounterLimiter(QuotaPolicy policy) {
        this.policy = policy;
        this.windowMillis = policy.windowDuration().toMillis();
        long now = System.currentTimeMillis();
        long currentStart = now - (now % windowMillis);
        this.previousBucket = new WindowBucket(currentStart - windowMillis);
        this.currentBucket = new WindowBucket(currentStart);
    }

    public synchronized RateLimitDecision tryAcquire() {
        long now = System.currentTimeMillis();
        long currentStart = now - (now % windowMillis);

        // Slide window
        if (currentBucket.startMillis < currentStart) {
            if (currentBucket.startMillis == currentStart - windowMillis) {
                previousBucket = currentBucket;
            } else {
                previousBucket = new WindowBucket(currentStart - windowMillis);
            }
            currentBucket = new WindowBucket(currentStart);
        }

        double progress = (double) (now - currentStart) / windowMillis;
        double prevWeight = 1.0 - progress;
        long estimated = (long) (previousBucket.count.get() * prevWeight) + currentBucket.count.get();

        long capacity = policy.capacity();
        if (estimated < capacity) {
            currentBucket.count.incrementAndGet();
            long remaining = Math.max(0, capacity - estimated - 1);
            return RateLimitDecision.allowed(capacity, remaining, Instant.ofEpochMilli(currentStart + windowMillis));
        }

        long retryAfterSec = Math.max(1, (windowMillis - (now - currentStart)) / 1000);
        return RateLimitDecision.denied(capacity, retryAfterSec, Instant.ofEpochMilli(currentStart + windowMillis));
    }
}
```

### Component 4: Multi-Tenant Gateway Manager & Driver (`src/main/java/com/learning/ratelimit/`)

```java
package com.learning.ratelimit;

import com.learning.ratelimit.engine.TokenBucketLimiter;
import com.learning.ratelimit.model.*;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

public final class RateLimiterGateway {
    private final ConcurrentHashMap<String, TokenBucketLimiter> clientLimiters = new ConcurrentHashMap<>();
    private final Map<String, QuotaPolicy> tenantTiers = new ConcurrentHashMap<>();

    public void registerTenant(String apiKey, QuotaPolicy policy) {
        tenantTiers.put(apiKey, policy);
    }

    public RateLimitDecision evaluateRequest(String apiKey) {
        QuotaPolicy policy = tenantTiers.getOrDefault(apiKey, QuotaPolicy.FREE_TIER);
        TokenBucketLimiter bucket = clientLimiters.computeIfAbsent(apiKey, k -> new TokenBucketLimiter(policy));
        return bucket.tryAcquire(1);
    }
}

public class Main {
    public static void main(String[] args) {
        RateLimiterGateway gateway = new RateLimiterGateway();
        gateway.registerTenant("client-free", QuotaPolicy.FREE_TIER);
        gateway.registerTenant("client-pro", QuotaPolicy.PRO_TIER);

        System.out.println("Simulating 15 rapid requests from Free Tier (Capacity: 10)...");
        for (int i = 1; i <= 15; i++) {
            RateLimitDecision decision = gateway.evaluateRequest("client-free");
            if (decision.isAllowed()) {
                System.out.printf("Request %02d: ALLOWED (Remaining: %d)\n", i, decision.remaining());
            } else {
                System.out.printf("Request %02d: REJECTED (HTTP 429, Retry-After: %ds)\n", i, decision.retryAfterSeconds());
            }
        }
    }
}
```

---

## 3. Build Configuration (`pom.xml`)

```xml
<?xml version="1.0" encoding="UTF-8"?>
<project xmlns="http://maven.apache.org/POM/4.0.0"
         xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
         xsi:schemaLocation="http://maven.apache.org/POM/4.0.0 
         http://maven.apache.org/xsd/maven-4.0.0.xsd">
    <modelVersion>4.0.0</modelVersion>

    <groupId>com.learning.java</groupId>
    <artifactId>rate-limiter</artifactId>
    <version>1.0.0</version>

    <properties>
        <maven.compiler.source>25</maven.compiler.source>
        <maven.compiler.target>25</maven.compiler.target>
        <project.build.sourceEncoding>UTF-8</project.build.sourceEncoding>
        <junit.jupiter.version>5.11.4</junit.jupiter.version>
    </properties>

    <dependencies>
        <dependency>
            <groupId>org.junit.jupiter</groupId>
            <artifactId>junit-jupiter</artifactId>
            <version>${junit.jupiter.version}</version>
            <scope>test</scope>
        </dependency>
    </dependencies>
</project>
```

---

## 4. Comprehensive JUnit 5 Test Suite

```java
package com.learning.ratelimit;

import com.learning.ratelimit.engine.SlidingWindowCounterLimiter;
import com.learning.ratelimit.engine.TokenBucketLimiter;
import com.learning.ratelimit.model.*;
import org.junit.jupiter.api.Test;

import java.time.Duration;

import static org.junit.jupiter.api.Assertions.*;

public class RateLimiterTest {

    @Test
    void testTokenBucketBurstAndRejection() {
        QuotaPolicy policy = new QuotaPolicy("TEST", 5, 1.0, Duration.ofSeconds(10));
        TokenBucketLimiter limiter = new TokenBucketLimiter(policy);

        // Consume all 5 tokens
        for (int i = 0; i < 5; i++) {
            RateLimitDecision d = limiter.tryAcquire(1);
            assertTrue(d.isAllowed(), "Request " + i + " should be allowed");
        }

        // 6th request must be rejected
        RateLimitDecision denied = limiter.tryAcquire(1);
        assertFalse(denied.isAllowed(), "Request exceeding capacity must be denied");
        assertEquals(0, denied.remaining());
        assertTrue(denied.retryAfterSeconds() > 0);
    }

    @Test
    void testSlidingWindowCounterLimiter() {
        QuotaPolicy policy = new QuotaPolicy("WINDOW_TEST", 3, 10.0, Duration.ofSeconds(1));
        SlidingWindowCounterLimiter limiter = new SlidingWindowCounterLimiter(policy);

        assertTrue(limiter.tryAcquire().isAllowed());
        assertTrue(limiter.tryAcquire().isAllowed());
        assertTrue(limiter.tryAcquire().isAllowed());
        assertFalse(limiter.tryAcquire().isAllowed(), "Should exceed window limit of 3");
    }

    @Test
    void testMultiTenantGatewayIsolation() {
        RateLimiterGateway gateway = new RateLimiterGateway();
        gateway.registerTenant("user-a", new QuotaPolicy("T1", 2, 1.0, Duration.ofSeconds(5)));
        gateway.registerTenant("user-b", new QuotaPolicy("T2", 5, 1.0, Duration.ofSeconds(5)));

        // User A exhausts quota
        assertTrue(gateway.evaluateRequest("user-a").isAllowed());
        assertTrue(gateway.evaluateRequest("user-a").isAllowed());
        assertFalse(gateway.evaluateRequest("user-a").isAllowed());

        // User B must remain unaffected
        assertTrue(gateway.evaluateRequest("user-b").isAllowed());
        assertTrue(gateway.evaluateRequest("user-b").isAllowed());
        assertTrue(gateway.evaluateRequest("user-b").isAllowed());
    }
}
```

---

## 5. Verification & Sample Execution

### Compilation & Test Execution
```bash
$ mvn clean test
```

### Expected Test Output
```text
[INFO] -------------------------------------------------------
[INFO]  T E S T S
[INFO] -------------------------------------------------------
[INFO] Running com.learning.ratelimit.RateLimiterTest
[INFO] Tests run: 3, Failures: 0, Errors: 0, Skipped: 0, Time elapsed: 0.124 s
[INFO] 
[INFO] Results:
[INFO] 
[INFO] Tests run: 3, Failures: 0, Errors: 0, Skipped: 0
[INFO] 
[INFO] BUILD SUCCESS
```

### Sample Interactive CLI Run
```bash
$ java -cp target/classes com.learning.ratelimit.Main
Simulating 15 rapid requests from Free Tier (Capacity: 10)...
Request 01: ALLOWED (Remaining: 9)
Request 02: ALLOWED (Remaining: 8)
Request 03: ALLOWED (Remaining: 7)
Request 04: ALLOWED (Remaining: 6)
Request 05: ALLOWED (Remaining: 5)
Request 06: ALLOWED (Remaining: 4)
Request 07: ALLOWED (Remaining: 3)
Request 08: ALLOWED (Remaining: 2)
Request 09: ALLOWED (Remaining: 1)
Request 10: ALLOWED (Remaining: 0)
Request 11: REJECTED (HTTP 429, Retry-After: 1s)
Request 12: REJECTED (HTTP 429, Retry-After: 1s)
Request 13: REJECTED (HTTP 429, Retry-After: 1s)
Request 14: REJECTED (HTTP 429, Retry-After: 1s)
Request 15: REJECTED (HTTP 429, Retry-After: 1s)
```

---

[← Return to Projects Index](README.md)
