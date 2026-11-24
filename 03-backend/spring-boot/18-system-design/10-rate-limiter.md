# 10: Designing a Distributed Rate Limiter with Spring Boot & Redis

| Metadata Field | Value |
|---|---|
| **Topic** | Distributed Rate Limiting, Sliding Window Counter & Token Bucket |
| **Spring Boot Version** | 3.4.x |
| **Java Version** | 21 / 25 LTS |
| **Target Scale** | Sub-Millisecond Decisions, Millions of Global Clients |
| **Key Middleware** | Redis Cluster (Lua Scripts & Sorted Sets), Spring Cloud Gateway, Resilience4j |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [Internals & Architecture](#3-internals--architecture)
4. [Syntax & Practical Usage](#4-syntax--practical-usage)
5. [5 Practical Examples](#5-5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Pitfalls & Anti-Patterns](#7-common-pitfalls--anti-patterns)
8. [Performance & Security Considerations](#8-performance--security-considerations)
9. [Comparison / Decision Matrix](#9-comparison--decision-matrix)
10. [Real-World Interview Q&A](#10-real-world-interview-qa)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference Cheatsheet](#12-summary--quick-reference-cheatsheet)
13. [Further Reading & Official Resources](#13-further-reading--official-resources)

---

## 1. What & Why

A distributed rate limiter protects backend services from volumetric Denial of Service (DoS) attacks, brute-force password cracking, API scraping, and cascading service outages caused by traffic spikes. It throttles incoming traffic to a safe, sustainable rate per client (identified by IP address, authenticated User ID, or API key).

In a distributed multi-node Spring Boot environment, in-memory rate limiting (e.g., standard Guava RateLimiter) is inadequate because each pod maintains independent memory counters. An attacker could bypass limits by distributing requests across 40 pods. A centralized, low-latency in-memory store (**Redis**) running atomic **Sliding Window Counter** or **Token Bucket** algorithms via Lua scripts guarantees globally accurate enforcement in under 1 millisecond.

---

## 2. Core Concepts

### 2.1 Rate Limiting Algorithms
1. **Token Bucket**: Tokens are continuously added to a bucket at a fixed rate up to a max capacity. Each request consumes one token. Allows burst traffic while maintaining an average rate.
2. **Fixed Window Counter**: Divides time into fixed intervals (e.g., 1 minute). Vulnerable to boundary spikes: twice the allowed traffic can hit within a 2-second window across a boundary.
3. **Sliding Window Log**: Stores timestamps of every request in a Redis Sorted Set (`ZSET`). Highly accurate, but high memory overhead for high-QPS endpoints.
4. **Sliding Window Counter (Industry Standard)**: A hybrid approximation combining the previous window's count with the current window's count based on the current timestamp percentage:
   $$\text{Current Count} = \text{Current Window Count} + \left( \text{Previous Window Count} \times \left(1 - \frac{\text{Elapsed Time in Current Window}}{\text{Window Size}}\right) \right)$$

### 2.2 Standard IETF RateLimit Headers
When rejecting or permitting requests, servers return standard HTTP headers:
- `RateLimit-Limit`: Maximum requests permitted per window.
- `RateLimit-Remaining`: Number of requests remaining in current window.
- `RateLimit-Reset`: Seconds remaining until quota refreshes.
- HTTP Status `429 Too Many Requests` when limit is exceeded.

---

## 3. Internals & Architecture

```
        [ Client API Request: /api/v1/orders ]
                          │
                          ▼
       ┌─────────────────────────────────────┐
       │   Spring Cloud Gateway / Filter     │
       │   1. Extract Key (IP / API Key)     │
       └──────────────────┬──────────────────┘
                          │
                          │ Check Rate Limit via Redis Lua
                          ▼
       ┌─────────────────────────────────────┐
       │      Redis Cluster (Lua Script)     │
       │  Sliding Window Counter Execution   │
       │  - Calculate current + prev window  │
       │  - Increment counter atomically     │
       └──────────────────┬──────────────────┘
                          │
         ┌────────────────┴────────────────┐
         ▼ (Limit Exceeded)                ▼ (Allowed)
   [ Return HTTP 429 ]              [ Forward to Downstream ]
   [ Retry-After: 12 ]              [ Microservice (Order)  ]
```

---

## 4. Syntax & Practical Usage

### 4.1 Sliding Window Counter Lua Script
```lua
local key = KEYS[1]
local limit = tonumber(ARGV[1])
local currentWindow = ARGV[2]
local prevWindow = ARGV[3]
local weight = tonumber(ARGV[4])

local currentCount = tonumber(redis.call('hget', key, currentWindow) or '0')
local prevCount = tonumber(redis.call('hget', key, prevWindow) or '0')

local estimatedCount = currentCount + (prevCount * weight)

if estimatedCount < limit then
    redis.call('hincrby', key, currentWindow, 1)
    redis.call('expire', key, 120) -- 2 minutes TTL
    return 1 -- ALLOWED
else
    return 0 -- REJECTED
end
```

---

## 5. 5 Practical Examples

### Example 1: Spring Data Redis Rate Limiter Service

```java
package com.example.ratelimit.service;

import org.springframework.core.io.ClassPathResource;
import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.data.redis.core.script.DefaultRedisScript;
import org.springframework.stereotype.Service;

import java.time.Instant;
import java.util.List;

@Service
public class RedisRateLimiterService {

    private final StringRedisTemplate redisTemplate;
    private final DefaultRedisScript<Long> slidingWindowScript;

    public RedisRateLimiterService(StringRedisTemplate redisTemplate) {
        this.redisTemplate = redisTemplate;
        this.slidingWindowScript = new DefaultRedisScript<>();
        this.slidingWindowScript.setLocation(new ClassPathResource("scripts/sliding_window.lua"));
        this.slidingWindowScript.setResultType(Long.class);
    }

    public record RateLimitDecision(boolean allowed, int limit, int remaining, long resetSeconds) {}

    public RateLimitDecision checkRateLimit(String clientKey, int limitPerMinute) {
        long nowMillis = Instant.now().toEpochMilli();
        long currentWindow = nowMillis / 60000;
        long prevWindow = currentWindow - 1;
        double weight = 1.0 - ((nowMillis % 60000) / 60000.0);

        String redisKey = "ratelimit:" + clientKey;

        Long result = redisTemplate.execute(
                slidingWindowScript,
                List.of(redisKey),
                String.valueOf(limitPerMinute),
                String.valueOf(currentWindow),
                String.valueOf(prevWindow),
                String.format("%.4f", weight)
        );

        boolean allowed = result != null && result == 1L;
        long resetSeconds = 60 - ((nowMillis / 1000) % 60);

        return new RateLimitDecision(allowed, limitPerMinute, allowed ? 10 : 0, resetSeconds);
    }
}
```

### Example 2: Non-Blocking Servlet Filter with Fail-Open Resilience

```java
package com.example.ratelimit.filter;

import com.example.ratelimit.service.RedisRateLimiterService;
import jakarta.servlet.FilterChain;
import jakarta.servlet.ServletException;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.http.HttpStatus;
import org.springframework.stereotype.Component;
import org.springframework.web.filter.OncePerRequestFilter;

import java.io.IOException;

@Component
public class DistributedRateLimitFilter extends OncePerRequestFilter {

    private static final Logger log = LoggerFactory.getLogger(DistributedRateLimitFilter.class);
    private final RedisRateLimiterService rateLimiterService;

    public DistributedRateLimitFilter(RedisRateLimiterService rateLimiterService) {
        this.rateLimiterService = rateLimiterService;
    }

    @Override
    protected void doFilterInternal(HttpServletRequest request, HttpServletResponse response, FilterChain filterChain)
            throws ServletException, IOException {

        String clientKey = extractClientIdentifier(request);

        try {
            // Check rate limit: 100 requests per minute
            var decision = rateLimiterService.checkRateLimit(clientKey, 100);

            // Populate standard IETF RateLimit headers
            response.setHeader("RateLimit-Limit", String.valueOf(decision.limit()));
            response.setHeader("RateLimit-Remaining", String.valueOf(decision.remaining()));
            response.setHeader("RateLimit-Reset", String.valueOf(decision.resetSeconds()));

            if (!decision.allowed()) {
                response.setStatus(HttpStatus.TOO_MANY_REQUESTS.value());
                response.setHeader("Retry-After", String.valueOf(decision.resetSeconds()));
                response.setContentType("application/json");
                response.getWriter().write("{\"error\":\"Too Many Requests. Quota exceeded.\"}");
                return;
            }

        } catch (Exception ex) {
            // Fail-Open Resilience: If Redis is temporarily down, log warning and let request pass through
            log.error("Rate limiter Redis cluster unavailable! Failing open to preserve availability: {}", ex.getMessage());
        }

        filterChain.doFilter(request, response);
    }

    private String extractClientIdentifier(HttpServletRequest request) {
        String apiKey = request.getHeader("X-API-KEY");
        if (apiKey != null && !apiKey.isBlank()) {
            return "key:" + apiKey;
        }
        return "ip:" + request.getRemoteAddr();
    }
}
```

### Example 3: Spring Cloud Gateway Redis Rate Limiter Route

```java
package com.example.ratelimit.config;

import org.springframework.cloud.gateway.filter.ratelimit.KeyResolver;
import org.springframework.cloud.gateway.filter.ratelimit.RedisRateLimiter;
import org.springframework.cloud.gateway.route.RouteLocator;
import org.springframework.cloud.gateway.route.builder.RouteLocatorBuilder;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.context.annotation.Primary;
import reactor.core.publisher.Mono;

@Configuration
public class GatewayRateLimiterConfig {

    @Bean
    public RedisRateLimiter redisRateLimiter() {
        // Replenish 50 tokens/sec, burst capacity up to 100 tokens
        return new RedisRateLimiter(50, 100);
    }

    @Bean
    @Primary
    public KeyResolver apiKeyResolver() {
        return exchange -> Mono.justOrEmpty(exchange.getRequest().getHeaders().getFirst("X-API-KEY"))
                .defaultIfEmpty(exchange.getRequest().getRemoteAddress().getAddress().getHostAddress());
    }

    @Bean
    public RouteLocator customRoutes(RouteLocatorBuilder builder, RedisRateLimiter rateLimiter, KeyResolver keyResolver) {
        return builder.routes()
                .route("order-service", r -> r.path("/api/v1/orders/**")
                        .filters(f -> f.requestRateLimiter(c -> c.setRateLimiter(rateLimiter).setKeyResolver(keyResolver)))
                        .uri("lb://order-service"))
                .build();
    }
}
```

### Example 4: User Tiered Rate Limiter Configuration

```java
package com.example.ratelimit.service;

import org.springframework.stereotype.Service;

@Service
public class TieredLimitProvider {

    public enum UserTier { FREE, PRO, ENTERPRISE }

    public int getPermittedRpm(UserTier tier) {
        return switch (tier) {
            case FREE -> 60;        // 1 req/sec
            case PRO -> 600;        // 10 req/sec
            case ENTERPRISE -> 6000;// 100 req/sec
        };
    }
}
```

### Example 5: Sliding Window Counter Lua Script File (`src/main/resources/scripts/sliding_window.lua`)

```lua
local key = KEYS[1]
local limit = tonumber(ARGV[1])
local currentWindow = ARGV[2]
local prevWindow = ARGV[3]
local weight = tonumber(ARGV[4])

local currentCount = tonumber(redis.call('hget', key, currentWindow) or '0')
local prevCount = tonumber(redis.call('hget', key, prevWindow) or '0')

local estimatedCount = currentCount + (prevCount * weight)

if estimatedCount < limit then
    redis.call('hincrby', key, currentWindow, 1)
    redis.call('expire', key, 120)
    return 1
else
    return 0
end
```

---

## 6. Best Practices

- **Adopt the Sliding Window Counter**: Fixed windows allow $2\times$ boundary bursts; Sliding Window Log consumes too much memory for high-QPS traffic. Sliding Window Counter provides high accuracy with minimal memory.
- **Fail-Open vs. Fail-Closed Strategy**: For standard user traffic, **fail-open** (allow traffic through if Redis is unreachable) to protect business revenue. For security endpoints (e.g. login OTP), **fail-closed** to prevent credential stuffing.
- **Return Informative IETF Headers**: Always emit `RateLimit-Limit`, `RateLimit-Remaining`, and `Retry-After` on every response so clients can back off cooperatively.
- **Use Multi-Tier Limits**: Apply both per-second burst limits (e.g., max 20 req/s) and daily quota limits (e.g., max 10,000 req/day).
- **Authenticate Before Rate Limiting**: Limit authenticated users by User ID or API Key rather than IP address, preventing corporate VPN users from sharing and exhausting a single IP quota.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
|---|---|---|
| **Local JVM In-Memory Limiting** | An attacker distributing requests across 40 pods gets $40\times$ the allowed quota. | Use a centralized Redis Cluster for global rate limiting. |
| **Fixed Window Counter** | Allows $2\times$ quota burst across boundary minutes (e.g. 100 req at 11:59:59 and 100 req at 12:00:01). | Use Sliding Window Counter algorithm. |
| **Failing Closed on Redis Outages** | A brief Redis network hiccup returns HTTP 500/429 to all customers, taking down the entire API. | Catch Redis exceptions and fail open for public endpoints. |
| **Limiting Exclusively by IP Address** | An entire corporate office behind NAT shares one IP; one user exhausts the quota for all colleagues. | Rate limit authenticated requests by API key or User ID. |
| **Separating Redis `GET` and `SET`** | Concurrency race condition: two threads read count = 99 and both increment, exceeding the limit. | Execute all checks and increments inside a single atomic Lua script. |
| **No Response Headers Returned** | Clients don't know their quota status and continue retrying blindly, aggravating load. | Return `RateLimit-Limit`, `RateLimit-Remaining`, and `Retry-After`. |

---

## 8. Performance & Security Considerations

### 8.1 Sub-Millisecond Redis Latency
Place the Redis Rate Limiter cluster in the same AWS Availability Zone / Kubernetes cluster as application nodes. Sub-millisecond latency is vital since every HTTP call incurs this roundtrip.

### 8.2 Sliding Window Counter Memory Footprint
A Redis Hash holding two integer fields consumes $\sim 60\text{ bytes}$.
For 1,000,000 active clients:
$$1,000,000 \times 60\text{ bytes} \approx 60\text{ MB RAM}$$
Sliding Window Counter is exceptionally lightweight and memory efficient.

---

## 9. Comparison / Decision Matrix

| Algorithm | Accuracy | Memory Consumption | Burst Handling | Implementation Complexity |
|---|---|---|---|---|
| **Token Bucket** | High | Low (2 numbers: tokens, lastTimestamp)| Excellent (Allows burst up to capacity) | Medium |
| **Fixed Window** | Low (Boundary issue) | Lowest (Single integer counter) | Poor (2x burst at boundary) | Low |
| **Sliding Window Log**| Perfect | Very High (1 timestamp per request)| Fair | High |
| **Sliding Window Counter**| Very High ($\sim 99\%$) | Low (Hash with 2 windows) | Good | Medium |

---

## 10. Real-World Interview Q&A

<details>
<summary><strong>1. Why is the Sliding Window Counter algorithm preferred over Fixed Window?</strong></summary>

Fixed Window algorithms reset counters at the start of each interval. An attacker can send 100 requests at 11:59:59 and 100 requests at 12:00:00, pushing 200 requests through in 2 seconds. Sliding Window Counter weights previous and current windows, preventing boundary spikes.
</details>

<details>
<summary><strong>2. How do you handle rate limiter failure (e.g., Redis cluster crash)?</strong></summary>

For public/business APIs, fail open: catch the Redis connection exception, log an alert, and allow the request through. For security-sensitive endpoints (e.g., password reset, OTP verification), fail closed to prevent brute-force attacks.
</details>

<details>
<summary><strong>3. Where should the rate limiter reside: API Gateway or Application Pod?</strong></summary>

Prefer placing rate limiting at the API Gateway (or Edge CDN) to drop abusive traffic before it traverses the internal network or consumes application CPU. Microservices can additionally implement internal rate limits for service-to-service fairness.
</details>

<details>
<summary><strong>4. How do you prevent race conditions when updating rate limit counters in Redis?</strong></summary>

Execute the check-and-increment operations inside a Redis Lua script. Redis executes Lua scripts atomically in a single thread, guaranteeing no interleaving requests can read stale counters.
</details>

<details>
<summary><strong>5. What standard HTTP status and headers must a rate limiter return?</strong></summary>

Return HTTP status `429 Too Many Requests` accompanied by `RateLimit-Limit`, `RateLimit-Remaining`, and `Retry-After` (seconds until client may retry).
</details>

<details>
<summary><strong>6. Why should you rate limit by API Key rather than Client IP?</strong></summary>

Large enterprise clients, universities, and mobile carriers operate thousands of devices behind shared NAT IP addresses. Limiting strictly by IP punishes all users for one bad actor. API keys isolate quotas per registered tenant.
</details>

<details>
<summary><strong>7. How does the Token Bucket algorithm differ from Leaky Bucket?</strong></summary>

Token Bucket allows bursts of traffic up to the bucket capacity while maintaining a steady average rate. Leaky Bucket enforces a smooth, constant outflow rate, queuing excess requests and dropping traffic if the queue overflows.
</details>

<details>
<summary><strong>8. How much memory does a 10M active user Sliding Window Counter require in Redis?</strong></summary>

Each user key stores a Redis Hash with two window counts ($\approx 60\text{ bytes}$). For 10 million active users, total RAM consumption is $\approx 600\text{ MB}$, easily fitting within a single modest Redis node.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Write a Redis Sliding Window Lua Script</strong></summary>

**Prompt**: Implement a Lua script that tracks timestamps in a Redis Hash and returns 1 if allowed, 0 if quota exceeded.

**Solution**:
Inspect Example 5: read previous and current window counts via `hget`, calculate weighted average, and call `hincrby` if below limit.
</details>

<details>
<summary><strong>Exercise 2: Implement Rate Limiting Servlet Filter</strong></summary>

**Prompt**: Write a filter extracting `X-API-KEY` and setting `RateLimit-Remaining` and `RateLimit-Limit` headers on the HTTP response.

**Solution**:
Inspect Example 2: invoke `rateLimiterService.checkRateLimit()` and call `response.setHeader()`.
</details>

<details>
<summary><strong>Exercise 3: Simulate 429 Too Many Requests in JUnit</strong></summary>

**Prompt**: Write a test executing 101 requests against an endpoint limited to 100 req/min and assert that the 101st request returns HTTP 429.

**Solution**:
Loop 100 times verifying `status().isOk()`. Call 101st time verifying `status().isTooManyRequests()` and `header().exists("Retry-After")`.
</details>

<details>
<summary><strong>Exercise 4: Implement Tier-Based Dynamic Limits</strong></summary>

**Prompt**: Look up the user's subscription tier (`FREE`, `PRO`) and apply 60 RPM or 600 RPM dynamically.

**Solution**:
```java
UserTier tier = userService.getTier(userId);
int limit = tieredLimitProvider.getPermittedRpm(tier);
var decision = rateLimiterService.checkRateLimit("user:" + userId, limit);
```
</details>

<details>
<summary><strong>Exercise 5: Configure Fail-Open Circuit Breaker for Redis</strong></summary>

**Prompt**: Use Resilience4j CircuitBreaker around the Redis rate limiter call with a fallback method returning `allowed = true`.

**Solution**:
```java
@CircuitBreaker(name = "rateLimiterRedis", fallbackMethod = "failOpenFallback")
public RateLimitDecision check(...) { ... }
public RateLimitDecision failOpenFallback(..., Throwable t) {
    return new RateLimitDecision(true, 100, 100, 0);
}
```
</details>

<details>
<summary><strong>Exercise 6: Configure Spring Cloud Gateway Token Bucket</strong></summary>

**Prompt**: Configure `application.yaml` for Spring Cloud Gateway route using Redis Token Bucket with replenishRate 10 and burstCapacity 20.

**Solution**:
```yaml
spring:
  cloud:
    gateway:
      routes:
        - id: api_route
          uri: http://localhost:8081
          filters:
            - name: RequestRateLimiter
              args:
                redis-rate-limiter.replenishRate: 10
                redis-rate-limiter.burstCapacity: 20
```
</details>

---

## 12. Summary & Quick Reference Cheatsheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                   DISTRIBUTED RATE LIMITER CHEATSHEET                  │
├────────────────────────────────────────────────────────────────────────┤
│ 1. Best Algorithm: Sliding Window Counter (prevents boundary spikes)   │
│ 2. Atomicity: Execute check + increment in Redis Lua script (<1ms)     │
│ 3. Fail-Open: Allow traffic through if Redis crashes (protect revenue) │
│ 4. Client Keys: Rate limit by API Key or User ID (not just IP)         │
│ 5. Headers: Return RateLimit-Limit, RateLimit-Remaining, Retry-After   │
│ 6. Response: HTTP 429 Too Many Requests when quota is exhausted        │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading & Official Resources

- [IETF Draft: RateLimit Header Fields for HTTP](https://datatracker.ietf.org/doc/html/draft-ietf-httpapi-ratelimit-headers)
- [Spring Cloud Gateway RequestRateLimiter Documentation](https://docs.spring.io/spring-cloud-gateway/reference/spring-cloud-gateway/gatewayfilter-factories/requestratelimiter-factory.html)
- [Redis Rate Limiting Patterns](https://redis.io/glossary/rate-limiting/)
- [Stripe: Scaling Your API with Rate Limiters](https://stripe.com/blog/rate-limiters)
