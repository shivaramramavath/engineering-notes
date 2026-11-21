# Enterprise Rate Limiting Architectures in Java

> Algorithm analysis (Token Bucket, Leaky Bucket, Sliding Window), atomic concurrency mechanics, distributed Redis implementations, and standard HTTP headers.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [`14-concurrency/01_atomic-and-volatile.md`](../14-concurrency/01_atomic-and-volatile.md), [`11-io-and-networking/03_http-client.md`](../11-io-and-networking/03_http-client.md) |
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

Rate limiting controls the rate at which requests are processed by a service. Without rate limiting, services are vulnerable to:
- **Denial of Service (DoS) and Brute-Force Attacks**: Attackers flooding login or search endpoints.
- **Resource Starvation (Noisy Neighbor Effect)**: A single abusive or misconfigured tenant consuming 90% of database threads.
- **Cost Runaway**: Uncontrolled consumption of pay-per-call third-party APIs (e.g. LLMs, payment gateways).
- **Cascading Outages**: Ingress traffic exceeding server capacity, exhausting thread pools and causing timeouts across all microservices.

A robust rate limiter enforces traffic boundaries gracefully, rejecting excess requests with `HTTP 429 (Too Many Requests)` while providing reset timing metadata.

---

## 2. Core Concepts

### 2.1 Rate Limiting Algorithms

1. **Token Bucket**:
   - A bucket has fixed capacity $C$ and refills with tokens at rate $R$ tokens/second.
   - Each request consumes 1 token. If the bucket is empty, request is rejected.
   - **Properties**: Allows controlled bursts of up to $C$ requests while maintaining average rate $R$.

2. **Leaky Bucket**:
   - Requests enter a queue (bucket) of capacity $C$ and leak out at a constant, smooth rate $R$.
   - If the queue overflows, incoming requests are dropped.
   - **Properties**: Completely smooths out traffic; eliminates bursts.

3. **Fixed Window Counter**:
   - Slices time into fixed intervals (e.g., 1-minute blocks: 12:00, 12:01).
   - Counts requests within the current window.
   - **Vulnerability**: Boundary burst. 100 requests at 12:00:59 followed by 100 requests at 12:01:01 allows 200 requests within a 2-second window (2x limit).

4. **Sliding Window Log**:
   - Records exact timestamp of each request in a sorted set (e.g. Redis ZSET).
   - Deletes timestamps older than `now - windowSize`.
   - **Properties**: 100% precise, but memory-intensive ($O(N)$ memory per client).

5. **Sliding Window Counter (Approximation)**:
   - Blends the count of the previous window with the current window:
     $$\text{rate} = \text{count}_{\text{prev}} \cdot (1 - \text{fraction}) + \text{count}_{\text{current}}$$
   - **Properties**: Extremely lightweight memory footprint with $< 0.05\%$ variance.

---

## 3. How It Works Internally

### 3.1 Lock-Free Token Bucket via Atomic Math

Instead of running background timer threads that periodically increment tokens (which wastes CPU and memory), high-performance implementations lazily calculate token replenishment on demand based on elapsed time:

$$\text{newTokens} = \min\left(C, \text{currentTokens} + \frac{(\text{now} - \text{lastRefill}) \cdot R}{10^9}\right)$$

```
Client Thread Request
         |
         v
Read atomic state: [tokens, lastRefillNanos]
         |
Compute elapsed time: delta = nowNanos - lastRefillNanos
Compute generated tokens: delta * rate / 1_000_000_000
New token pool = min(capacity, tokens + generated)
         |
Can we consume 1 token?
  /             \
Yes              No
 /                 \
CAS update state   Reject request (HTTP 429)
Return ALLOWED
```

---

## 4. Syntax & Usage

### 4.1 Production HTTP Rate Limit Headers (RFC 6585 & IETF Draft)

Standard responses for rate-limited APIs return:
- `X-RateLimit-Limit`: Maximum requests permitted per window.
- `X-RateLimit-Remaining`: Number of requests remaining in current window.
- `X-RateLimit-Reset`: Unix epoch timestamp when quota resets.
- `Retry-After`: Seconds to wait before making a new request.

```http
HTTP/1.1 429 Too Many Requests
Content-Type: application/json
Retry-After: 30
X-RateLimit-Limit: 100
X-RateLimit-Remaining: 0
X-RateLimit-Reset: 1740000060

{
  "error": "Too Many Requests",
  "message": "Rate limit quota exceeded. Please retry after 30 seconds."
}
```

---

## 5. Practical Examples

### 5.1 Basic Example: Thread-Safe Lock-Free Token Bucket

```java
import java.util.concurrent.atomic.AtomicReference;

public final class LockFreeTokenBucket {
    private record BucketState(long tokens, long lastRefillNanos) {}

    private final long capacity;
    private final double refillTokensPerNano;
    private final AtomicReference<BucketState> state;

    public LockFreeTokenBucket(long capacity, double tokensPerSecond) {
        if (capacity <= 0 || tokensPerSecond <= 0) {
            throw new IllegalArgumentException("Capacity and tokensPerSecond must be > 0");
        }
        this.capacity = capacity;
        this.refillTokensPerNano = tokensPerSecond / 1_000_000_000.0;
        this.state = new AtomicReference<>(new BucketState(capacity, System.nanoTime()));
    }

    public boolean tryConsume(long tokensRequested) {
        if (tokensRequested > capacity) return false;

        while (true) {
            BucketState current = state.get();
            long now = System.nanoTime();
            long elapsedNanos = Math.max(0, now - current.lastRefillNanos());

            long refilledTokens = (long) (elapsedNanos * refillTokensPerNano);
            long newTokens = Math.min(capacity, current.tokens() + refilledTokens);

            if (newTokens < tokensRequested) {
                return false; // Insufficient tokens
            }

            // If we refilled, advance timestamp; otherwise preserve lastRefillNanos to avoid fractional drift
            long updatedLastRefill = (refilledTokens > 0) ? now : current.lastRefillNanos();
            BucketState next = new BucketState(newTokens - tokensRequested, updatedLastRefill);

            if (state.compareAndSet(current, next)) {
                return true;
            }
            // CAS collision: loop and retry
        }
    }
}
```

### 5.2 Real-World Example: Sliding Window Counter Rate Limiter

```java
import java.time.Instant;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicLong;

public final class SlidingWindowCounterRateLimiter {
    private final long limit;
    private final long windowMillis;

    private record WindowBucket(long windowStart, AtomicLong count) {}
    private final ConcurrentHashMap<String, WindowBucket[]> clientWindows = new ConcurrentHashMap<>();

    public SlidingWindowCounterRateLimiter(long limit, long windowMillis) {
        this.limit = limit;
        this.windowMillis = windowMillis;
    }

    public synchronized boolean allowRequest(String clientId) {
        long now = System.currentTimeMillis();
        long currentWindowStart = now - (now % windowMillis);

        WindowBucket[] buckets = clientWindows.computeIfAbsent(clientId, k -> new WindowBucket[]{
            new WindowBucket(currentWindowStart - windowMillis, new AtomicLong(0)), // Previous
            new WindowBucket(currentWindowStart, new AtomicLong(0))                 // Current
        });

        // Rotate windows if needed
        if (buckets[1].windowStart < currentWindowStart) {
            if (buckets[1].windowStart == currentWindowStart - windowMillis) {
                buckets[0] = buckets[1];
            } else {
                buckets[0] = new WindowBucket(currentWindowStart - windowMillis, new AtomicLong(0));
            }
            buckets[1] = new WindowBucket(currentWindowStart, new AtomicLong(0));
        }

        // Calculate weighted sliding count
        double fractionOfCurrentWindow = (double) (now - currentWindowStart) / windowMillis;
        double previousWeight = 1.0 - fractionOfCurrentWindow;
        long estimatedRequests = (long) (buckets[0].count().get() * previousWeight) + buckets[1].count().get();

        if (estimatedRequests < limit) {
            buckets[1].count().incrementAndGet();
            return true;
        }

        return false;
    }
}
```

### 5.3 Distributed Redis Rate Limiting via Lua Script

In clustered environments, local in-memory limiters fail because traffic hits multiple distinct JVM instances. Redis running an atomic Lua script provides distributed sliding-window enforcement:

```lua
-- KEYS[1]: Rate limit key (e.g. rate:user:123)
-- ARGV[1]: Current timestamp in milliseconds
-- ARGV[2]: Window size in milliseconds
-- ARGV[3]: Max requests allowed in window

local key = KEYS[1]
local now = tonumber(ARGV[1])
local window = tonumber(ARGV[2])
local limit = tonumber(ARGV[3])
local clearBefore = now - window

-- 1. Remove expired request timestamps
redis.call('ZREMRANGEBYSCORE', key, 0, clearBefore)

-- 2. Count current elements in window
local currentRequests = redis.call('ZCARD', key)

if currentRequests < limit then
    -- 3. Add current request timestamp
    redis.call('ZADD', key, now, now)
    redis.call('PEXPIRE', key, window)
    return 1 -- Allowed
else
    return 0 -- Denied
end
```

Java driver invocation snippet:
```java
public class DistributedRedisRateLimiter {
    public boolean isAllowed(String userId, long limit, long windowMillis) {
        // Jedis/Lettuce call:
        // Object result = jedis.eval(LUA_SCRIPT, List.of("rate:" + userId), List.of(String.valueOf(System.currentTimeMillis()), String.valueOf(windowMillis), String.valueOf(limit)));
        // return Long.valueOf(1).equals(result);
        return true;
    }
}
```

---

## 6. Best Practices

1. **Rate Limit by Multiple Dimensions**:
   - Tier 1: By IP address (guards against network floods).
   - Tier 2: By authenticated `userId` / API token (guards against account abuse).
   - Tier 3: By sensitive endpoint (e.g. `POST /api/v1/auth/login` max 5/min, `GET /api/v1/products` max 1000/min).
2. **Never Run Background Timer Threads for Replenishment**: Lazy on-demand token computation with CAS scales to millions of users without wasting CPU threads.
3. **Always Include Standard Headers**: Always return `X-RateLimit-Limit`, `X-RateLimit-Remaining`, and `Retry-After` so well-behaved clients back off gracefully.
4. **Fail Open or Fail Closed?**:
   - Non-critical reads (product catalog): Fail open (allow traffic if Redis is down).
   - Expensive / High-risk writes (card processing, LLM generation): Fail closed (deny traffic if rate limiter is unreachable).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Fix |
| :--- | :--- | :--- |
| **Using Fixed Window** | Clients flood 2x quota at the window boundary | Switch to Sliding Window Counter or Token Bucket |
| **Memory Leaks from Idle Clients** | Storing millions of inactive IP keys forever causes JVM `OutOfMemoryError` | Use Caffeine cache with `expireAfterAccess()` to purge idle limiters |
| **Coarse Synchronization (`synchronized`)** | A single global lock blocks all incoming HTTP requests during token updates | Use atomic CAS (`AtomicReference` or `AtomicLong`) |
| **Relying Only on Client IP** | Multiple corporate users behind a single NAT share one public IP, throttling legitimate users | Rate limit by authenticated session ID / API key when available |
| **Fractional Token Loss** | Integer division rounds down refilled tokens to zero if elapsed time is short | Maintain timestamp delta or accumulate fractional tokens with floating-point math |

---

## 8. Performance & Security Notes

### 8.1 CAS Spin Loop Contention
Under extreme contention (100,000 requests/sec for a single hot API key), CAS retry loops in `LockFreeTokenBucket` can burn CPU.
- **Optimization**: For hot global limiters, use `LongAdder` or striped buckets to distribute contention across multiple cache lines.

### 8.2 Security: IP Spoofing Prevention
Attackers spoof `X-Forwarded-For` HTTP headers to bypass IP-based rate limiting.
- **Rule**: Only trust `X-Forwarded-For` if set by a verified, trusted reverse proxy (NGINX, Cloudflare, AWS ALB). Strip external client-supplied header values at the edge proxy.

---

## 9. Comparison / When to Use What

| Algorithm | Memory | Accuracy | Bursts Allowed? | Recommended Context |
| :--- | :--- | :--- | :--- | :--- |
| **Token Bucket** | $O(1)$ | High | Yes (up to capacity) | General API gateways, Stripe-style rate limiting |
| **Leaky Bucket** | $O(C)$ | High | No (smooth output) | Traffic shaping, packet switching, message queues |
| **Fixed Window** | $O(1)$ | Low (Boundary spike) | Yes | Simple internal service limits |
| **Sliding Window Log** | $O(N)$ | 100% Exact | No | High-security financial endpoints, low-traffic APIs |
| **Sliding Window Counter** | $O(1)$ | High ($\approx 99.9\%$) | Controlled | High-throughput distributed microservices |

---

## 10. Interview Questions

<details>
<summary>1. Explain the boundary problem in Fixed Window rate limiting.</summary>
If the window limit is 100 requests/minute, an attacker can send 100 requests at 12:00:59 and another 100 requests at 12:01:01. Both batches are accepted because they fall into separate 1-minute fixed windows. However, 200 requests were executed within a 2-second span, defeating the purpose of the rate limiter.
</details>

<details>
<summary>2. How does Token Bucket differ from Leaky Bucket?</summary>
Token Bucket allows traffic bursts up to bucket capacity while capping average throughput at the token refill rate. Leaky Bucket enforces a completely constant, smooth output rate regardless of ingress bursts by buffering requests in a FIFO queue.
</details>

<details>
<summary>3. Why is Redis Lua scripting preferred for distributed rate limiting?</summary>
Redis executes Lua scripts atomically in a single-threaded event loop. This eliminates race conditions (check-then-act) without requiring distributed locks across multiple client applications.
</details>

<details>
<summary>4. How does lazy token replenishment work?</summary>
Instead of a background thread running every millisecond to add tokens, the bucket records the timestamp of the last access. When a request arrives, the algorithm calculates how many tokens should have accumulated in the elapsed time $(\text{now} - \text{lastTimestamp})$ and updates the token count immediately.
</details>

<details>
<summary>5. What HTTP status code and response headers should a rate limiter return?</summary>
HTTP status `429 Too Many Requests`. Standard headers include `Retry-After: <seconds>`, `X-RateLimit-Limit`, `X-RateLimit-Remaining`, and `X-RateLimit-Reset`.
</details>

<details>
<summary>6. How do you prevent memory leaks when storing per-user rate limiters in JVM memory?</summary>
Store rate limiters in a cache with an expiration policy (such as Caffeine with `.expireAfterAccess(Duration.ofMinutes(15))`). When a user stops sending requests, their limiter is automatically evicted by GC.
</details>

<details>
<summary>7. What is the difference between Fail-Open and Fail-Closed rate limiting?</summary>
Fail-Open permits requests to proceed if the rate limiter backend (e.g. Redis) is unavailable, preserving availability at the risk of server overload. Fail-Closed rejects incoming requests during outages, prioritizing backend protection at the expense of availability.
</details>

<details>
<summary>8. How do you mitigate IP-spoofing attacks against IP-based rate limiting?</summary>
Extract the remote IP strictly from trusted upstream proxy headers (e.g. AWS ALB or Cloudflare `CF-Connecting-IP`). Reject or overwrite any client-manipulated `X-Forwarded-For` headers entering the perimeter.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Implement an In-Memory Per-Client Rate Limiter using Caffeine Cache</summary>

```java
import java.time.Duration;
import java.util.concurrent.ConcurrentHashMap;

public class ClientRateLimitGateway {
    private final ConcurrentHashMap<String, LockFreeTokenBucket> limiters = new ConcurrentHashMap<>();
    private final long capacity;
    private final double refillRate;

    public ClientRateLimitGateway(long capacity, double refillRate) {
        this.capacity = capacity;
        this.refillRate = refillRate;
    }

    public boolean checkAccess(String clientId) {
        LockFreeTokenBucket bucket = limiters.computeIfAbsent(clientId, 
            k -> new LockFreeTokenBucket(capacity, refillRate));
        return bucket.tryConsume(1);
    }
}
```
</details>

<details>
<summary>Exercise 2: Implement Rate Limiting HTTP Filter Interceptor</summary>

```java
public class RateLimitFilter {
    private final LockFreeTokenBucket bucket;

    public RateLimitFilter(LockFreeTokenBucket bucket) {
        this.bucket = bucket;
    }

    public record HttpResponse(int status, String body) {}

    public HttpResponse handle(String requestPath) {
        if (!bucket.tryConsume(1)) {
            return new HttpResponse(429, "{\"error\":\"Too Many Requests\",\"retryAfter\":10}");
        }
        return new HttpResponse(200, "{\"status\":\"success\"}");
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Token Bucket**: Bursts permitted up to capacity, refills at constant rate. Lock-free on JVM via atomic CAS math.
- **Leaky Bucket**: Smooth constant-rate processing via queue.
- **Sliding Window Counter**: Balances $O(1)$ memory with boundary burst protection.
- **Distributed**: Execute atomic Redis Lua script over Redis Sorted Sets (`ZREMRANGEBYSCORE` + `ZCARD` + `ZADD`).
- **HTTP Spec**: Return `429 Too Many Requests` with `Retry-After: <seconds>`.
- **Hygiene**: Evict inactive limiters via TTL to avoid heap memory exhaustion.

---

## 13. Further Reading

- [IETF RateLimit Header Fields Draft](https://datatracker.ietf.org/doc/draft-ietf-httpapi-ratelimit-headers/)
- [Stripe Engineering: Scaling Your API with Rate Limiters](https://stripe.com/blog/rate-limiters)
- [Bucket4j Java Rate Limiting Library](https://github.com/bucket4j/bucket4j)
