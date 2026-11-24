# 20. Rate Limiting, Brute Force Mitigation & Denial-of-Service Defense in Spring Boot

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | `01-spring-security-architecture.md`, `02-authentication-fundamentals.md`, Redis Basics |
| **Estimated Reading Time** | 40 minutes |
| **Status** | Complete |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 21, Spring Boot 3.4.1, Redis 7.2, Bucket4j 8.10 |

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
Unprotected login endpoints, public registration forms, and API routes are prime targets for automated attacks:
- **Credential Stuffing & Brute Force**: Automated botnets cycle through millions of stolen username/password combinations to compromise user accounts.
- **Denial of Service (DoS)**: Malicious actors flood CPU-intensive endpoints (e.g., password hashing with Argon2 or report generation) to exhaust server resources.
- **Resource Starvation / API Abuse**: Aggressive web scrapers or poorly configured partner clients consume disproportionate bandwidth, degrading service for legitimate users.

**Rate Limiting and Brute Force Defense** constrain the frequency of incoming requests based on client identifiers (IP address, authenticated user ID, or API key). When thresholds are exceeded, the server responds with **HTTP 429 Too Many Requests** or temporarily locks the account.

### Historical Evolution
- **Early Web APIs**: Developers used naive in-memory hash maps or simple database counters, which failed under distributed multi-server deployments and suffered from severe race conditions.
- **Bucket4j & Redis (2015-Present)**: Standardized the **Token Bucket** algorithm backed by Redis atomicity (Lua scripts or Redisson locks), providing lock-free distributed rate limiting.
- **Modern Spring Boot 3.x/4.x**: Implemented via reactive filters, standard `HandlerInterceptor` chains, or Spring Cloud Gateway filters with RFC-standard headers (`RateLimit-Limit`, `RateLimit-Remaining`, `RateLimit-Reset`, `Retry-After`).

### Real-World Relevance
Implementing multi-tiered rate limiting is essential to satisfy OWASP Top 10 (A04: Insecure Design, A07: Identification and Authentication Failures) and protect cloud infrastructure from runaway compute costs.

---

## 2. Core Concepts

### Architectural Contracts
- **Token Bucket Algorithm**: A bucket of fixed capacity $C$ continuously accumulates tokens at a fill rate $R$. Each incoming request consumes 1 token. If the bucket is empty, the request is rejected immediately. Allows bursts up to $C$.
- **Leaky Bucket Algorithm**: Requests enter a queue and are processed at a strictly constant rate. Smooths out traffic spikes.
- **Fixed Window Counter**: Divides time into fixed intervals (e.g., 1 minute). Vulnerable to boundary bursts (2x limit across window boundaries).
- **Sliding Window Log**: Tracks exact request timestamps in a sorted set (Redis ZSET). Highly accurate but memory-intensive.
- **Sliding Window Counter**: Combines the fixed window counter with a weighted calculation of the previous window, balancing precision and low memory.
- **Account Lockout Policy**: Tracks failed authentication attempts per username. Exceeding $N$ attempts locks the account for $M$ minutes.

### The Mental Model
```
Client Request (IP: 203.0.113.42)
         │
         ▼
[ RateLimitingFilter / FilterChain ]
         │
         ├── 1. Resolve Partition Key (Client IP or Authenticated User)
         │
         ├── 2. Query Distributed Token Bucket (Redis via Bucket4j)
         │      ├── Tokens available in bucket?
         │      │     ├─ YES: Consume 1 token ──► Proceed to Controller (200 OK)
         │      │     │                           Add Headers:
         │      │     │                             RateLimit-Remaining: 9
         │      │     │
         │      │     └─ NO: Bucket empty! ────► Reject Request (429 Too Many Requests)
         │      │                                 Add Headers:
         │      │                                   Retry-After: 45
         │      │                                   RateLimit-Reset: 45
```

### Invariants
1. Rate limit counters must be stored in a shared distributed datastore (Redis) in clustered deployments to prevent clients from bypassing limits across server replicas.
2. Rate limit operations in Redis must be atomic (via Lua scripts or atomic transactions) to prevent race conditions during concurrent requests.
3. Login failures must track both IP address (to block brute force) and username (to block distributed credential stuffing).

---

## 3. How It Works Internally

### The Token Bucket Refill Formula
Let:
- $C = \text{Capacity (maximum burst allowed)}$
- $R = \text{Refill rate (tokens per second)}$
- $T_{\text{last}} = \text{Timestamp of previous request}$
- $T_{\text{now}} = \text{Timestamp of current request}$
- $B_{\text{last}} = \text{Token count at } T_{\text{last}}$

When a request arrives at $T_{\text{now}}$, the new token balance $B_{\text{now}}$ is computed dynamically without background timer threads:
$$\Delta T = T_{\text{now}} - T_{\text{last}}$$
$$B_{\text{refilled}} = \min(C, B_{\text{last}} + \Delta T \times R)$$
$$\text{If } B_{\text{refilled}} \ge 1 \implies \text{Consume 1 token, Allow request}$$
$$\text{If } B_{\text{refilled}} < 1 \implies \text{Deny request (HTTP 429)}$$

---

## 4. Syntax & Usage

### 1. Starter Dependencies (`pom.xml`)
```xml
<dependency>
    <groupId>com.bucket4j</groupId>
    <artifactId>bucket4j-redis</artifactId>
    <version>8.10.1</version>
</dependency>
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-data-redis</artifactId>
</dependency>
```

---

## 5. Practical Examples

### Example 1: In-Memory Token Bucket Filter for Public Endpoints
Using Bucket4j for fast in-memory rate limiting on standalone instances.

```java
package com.example.security.ratelimit;

import io.github.bucket4j.Bandwidth;
import io.github.bucket4j.Bucket;
import io.github.bucket4j.Refill;
import jakarta.servlet.FilterChain;
import jakarta.servlet.ServletException;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import org.springframework.http.HttpStatus;
import org.springframework.stereotype.Component;
import org.springframework.web.filter.OncePerRequestFilter;

import java.io.IOException;
import java.time.Duration;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

@Component
public class InMemoryIpRateLimitingFilter extends OncePerRequestFilter {

    private final Map<String, Bucket> buckets = new ConcurrentHashMap<>();

    private Bucket createNewBucket() {
        // Capacity = 20 tokens, Refills 10 tokens every minute
        Bandwidth limit = Bandwidth.classic(20, Refill.greedy(10, Duration.ofMinutes(1)));
        return Bucket.builder().addLimit(limit).build();
    }

    @Override
    protected void doFilterInternal(HttpServletRequest request, HttpServletResponse response, FilterChain filterChain)
            throws ServletException, IOException {

        if (!request.getRequestURI().startsWith("/api/public/")) {
            filterChain.doFilter(request, response);
            return;
        }

        String clientIp = resolveClientIp(request);
        Bucket bucket = buckets.computeIfAbsent(clientIp, k -> createNewBucket());

        if (bucket.tryConsume(1)) {
            response.setHeader("X-Rate-Limit-Remaining", String.valueOf(bucket.getAvailableTokens()));
            filterChain.doFilter(request, response);
        } else {
            response.setStatus(HttpStatus.TOO_MANY_REQUESTS.value());
            response.setHeader("Retry-After", "60");
            response.setContentType("application/json");
            response.getWriter().write("{\"error\": \"Too many requests. Please retry in 60 seconds.\"}");
        }
    }

    private String resolveClientIp(HttpServletRequest request) {
        String xForwardedFor = request.getHeader("X-Forwarded-For");
        if (xForwardedFor != null && !xForwardedFor.isBlank()) {
            return xForwardedFor.split(",")[0].trim();
        }
        return request.getRemoteAddr();
    }
}
```

### Example 2: Distributed Rate Limiting with Redis & Redisson
In clustered multi-instance environments, share bucket state across all pods using Redis.

```java
package com.example.security.ratelimit;

import io.github.bucket4j.Bandwidth;
import io.github.bucket4j.BucketConfiguration;
import io.github.bucket4j.Refill;
import io.github.bucket4j.distributed.proxy.ProxyManager;
import org.springframework.stereotype.Service;

import java.time.Duration;
import java.util.function.Supplier;

@Service
public class DistributedRateLimitService {

    private final ProxyManager<String> proxyManager;

    public DistributedRateLimitService(ProxyManager<String> proxyManager) {
        this.proxyManager = proxyManager;
    }

    public boolean tryConsume(String key, int tokens) {
        Supplier<BucketConfiguration> configSupplier = () -> BucketConfiguration.builder()
            .addLimit(Bandwidth.classic(100, Refill.intervally(100, Duration.ofMinutes(1))))
            .build();

        return proxyManager.builder().build(key, configSupplier).tryConsume(tokens);
    }
}
```

### Example 3: Account Lockout Service for Brute Force Login Protection
Locking a user account for 15 minutes after 5 consecutive failed login attempts.

```java
package com.example.security.auth;

import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.stereotype.Service;

import java.time.Duration;

@Service
public class LoginAttemptService {

    private static final int MAX_ATTEMPTS = 5;
    private static final Duration LOCK_TIME = Duration.ofMinutes(15);
    private final StringRedisTemplate redisTemplate;

    public LoginAttemptService(StringRedisTemplate redisTemplate) {
        this.redisTemplate = redisTemplate;
    }

    public void loginSucceeded(String username) {
        redisTemplate.delete("login:failed:" + username);
    }

    public void loginFailed(String username) {
        String key = "login:failed:" + username;
        Long attempts = redisTemplate.opsForValue().increment(key);
        if (attempts != null && attempts == 1) {
            redisTemplate.expire(key, LOCK_TIME);
        }
    }

    public boolean isBlocked(String username) {
        String key = "login:failed:" + username;
        String val = redisTemplate.opsForValue().get(key);
        if (val != null) {
            return Integer.parseInt(val) >= MAX_ATTEMPTS;
        }
        return false;
    }
}
```

### Example 4: Failure Anti-Pattern & Production Fix
**Anti-Pattern**: Using client IP addresses exclusively to protect against brute force login attacks.

```java
// BUGGY: Attackers use botnets with rotating IP addresses!
// Credential stuffing against "admin" succeeds because each attempt comes from a different IP!
if (rateLimiter.isIpBlocked(clientIp)) {
    throw new BlockedException();
}
```

**Production Fix**:
Enforce dual-key rate limiting: track limits both by **Client IP** (to block IP flooding) and by **Target Username** (to block distributed credential stuffing).

```java
// SECURE: Dual-dimension protection
boolean ipAllowed = rateLimitService.tryConsume("ip:" + clientIp, 1);
boolean userAllowed = rateLimitService.tryConsume("user:" + username, 1);

if (!ipAllowed || !userAllowed) {
    throw new TooManyRequestsException("Account temporarily throttled due to high activity.");
}
```

### Example 5: High-Performance Non-Blocking WebFlux Reactive Rate Limiter
Enforcing rate limits asynchronously in Spring Boot WebFlux.

```java
package com.example.security.reactive;

import org.springframework.core.io.buffer.DataBuffer;
import org.springframework.http.HttpStatus;
import org.springframework.http.MediaType;
import org.springframework.stereotype.Component;
import org.springframework.web.server.ServerWebExchange;
import org.springframework.web.server.WebFilter;
import org.springframework.web.server.WebFilterChain;
import reactor.core.publisher.Mono;

import java.nio.charset.StandardCharsets;

@Component
public class ReactiveRateLimitingFilter implements WebFilter {

    @Override
    public Mono<Void> filter(ServerWebExchange exchange, WebFilterChain chain) {
        String path = exchange.getRequest().getURI().getPath();
        if (path.startsWith("/api/public/")) {
            boolean allowed = checkRateLimit(exchange);
            if (!allowed) {
                exchange.getResponse().setStatusCode(HttpStatus.TOO_MANY_REQUESTS);
                exchange.getResponse().getHeaders().setContentType(MediaType.APPLICATION_JSON);
                byte[] bytes = "{\"error\":\"Rate limit exceeded\"}".getBytes(StandardCharsets.UTF_8);
                DataBuffer buffer = exchange.getResponse().bufferFactory().wrap(bytes);
                return exchange.getResponse().writeWith(Mono.just(buffer));
            }
        }
        return chain.filter(exchange);
    }

    private boolean checkRateLimit(ServerWebExchange exchange) {
        // Non-blocking reactive token check
        return true;
    }
}
```

---

## 6. Best Practices

1. **Use Multi-Tiered Rate Limiting**: Apply global IP limits (e.g., 1000 req/min), route-specific limits (e.g., login: 5 req/min), and authenticated user limits (e.g., 100 req/min).
2. **Standardize HTTP Headers**: Emit standard rate limit headers (`RateLimit-Limit`, `RateLimit-Remaining`, `RateLimit-Reset`, `Retry-After`).
3. **Use Redis in Clustered Deployments**: Never rely on in-memory maps in multi-replica container environments.
4. **Guard Against Spoofed `X-Forwarded-For`**: Only trust proxy headers from validated reverse proxy IP addresses.
5. **Implement Progressive Delays (Tarpitting)**: Introduce artificial latency delays (e.g., 1 second) on successive failed login attempts to exhaust attacker bot resources.
6. **Graceful Degradation on Redis Outages**: If Redis fails, fail open for read-only routes but fail closed for sensitive authentication routes.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| In-memory limits in Kubernetes | Each pod replica has its own bucket; clients cycle between pods to bypass limits. | Use distributed Redis-backed buckets. |
| Trusting unverified `X-Forwarded-For` | Attackers spoof `X-Forwarded-For: 1.1.1.1` to bypass IP-based rate limiting entirely. | Configure trusted proxy subnets or use Spring Boot's forwarded header filter. |
| Fixed window boundary spikes | Attackers send full quota at 00:59 and again at 01:00, achieving 2x burst rate. | Use Token Bucket or Sliding Window algorithms. |
| Unbounded in-memory map growth | Tracking millions of unique random IPs leaks memory, causing JVM OutOfMemory errors. | Evict idle buckets using TTL caches (Caffeine / Redis TTL). |
| Locking entire IPs on login failure | Legitimate users behind corporate NAT proxies get blocked when one user fails login. | Rate-limit login failures by username + IP combination. |
| Missing `Retry-After` header | Clients aggressively retry immediately upon receiving HTTP 429, worsening server load. | Include standard `Retry-After` header indicating wait duration. |

---

## 8. Performance & Security Notes

- **Redis Lua Script Overhead**: Redis executes rate-limiting Lua scripts in less than 0.5 milliseconds. In high-throughput systems, use local Redis clusters colocated in the same availability zone.
- **Virtual Threads Integration**: When performing distributed Redis rate-limit checks via Jedis or Lettuce, Java 21 Virtual Threads unmount cleanly during socket I/O without exhausting OS carrier threads.
- **Tarpitting Risks**: Artificially sleeping worker threads (`Thread.sleep()`) to slow down attackers consumes thread pool capacity. On traditional thread-per-request models, this causes thread starvation; on Java 21 Virtual Threads, tarpitting is safe and scalable.

---

## 9. Comparison / When to Use What

| Algorithm | Burst Friendly? | Memory Footprint | Computational Cost | Best Use Case |
|---|---|---|---|---|
| **Token Bucket (Bucket4j)**| **Yes (Up to Capacity)** | Very Low | Minimal (Mathematical formula)| General API rate limiting |
| **Leaky Bucket** | No (Smooths traffic) | Low | Moderate (Queue tracking) | Outbound webhook dispatchers |
| **Fixed Window** | No (Boundary bursts) | Extremely Low | Minimal (Simple counter) | Basic daily/hourly quotas |
| **Sliding Window Log** | Yes | High (Stores every timestamp)| High (ZSET range queries) | Strict compliance audits |

---

## 10. Interview Questions

<details>
<summary>1. How does the Token Bucket algorithm handle traffic bursts compared to the Leaky Bucket?</summary>

**Answer:**
The Token Bucket algorithm allows traffic bursts up to the maximum capacity of the bucket. If tokens have accumulated during idle periods, a client can execute a burst of requests immediately until tokens are exhausted.
The Leaky Bucket algorithm processes requests at a strictly constant rate regardless of bursts, smoothing out spikes by queuing incoming requests or dropping overflow traffic.
</details>

<details>
<summary>2. Why are in-memory rate limiters ineffective in cloud Kubernetes deployments?</summary>

**Answer:**
In Kubernetes, applications run across multiple pod replicas behind a load balancer. If an in-memory bucket is used, each pod maintains its own isolated counter. An attacker can distribute requests across all pods, effectively multiplying their allowed quota by the number of running replicas. Distributed storage (Redis) is required to maintain a single synchronized counter across all pods.
</details>

<details>
<summary>3. What is the boundary burst problem in Fixed Window rate limiting?</summary>

**Answer:**
In a fixed 1-minute window allowing 100 requests, an attacker can send 100 requests at 00:59 (the end of the first window) and another 100 requests at 01:00 (the start of the second window). In a 2-second span, the server processes 200 requests (2x the intended rate), potentially overwhelming backend resources. Sliding window or token bucket algorithms eliminate this issue.
</details>

<details>
<summary>4. How does Bucket4j compute available tokens without running background timer threads?</summary>

**Answer:**
Bucket4j uses a lazy evaluation mathematical formula:
When a request arrives at timestamp $T_{\text{now}}$, it calculates the elapsed time $\Delta T$ since the last request ($T_{\text{last}}$). It multiplies $\Delta T$ by the fill rate to determine how many new tokens should be added, caps the total at the bucket capacity, subtracts the requested tokens, and records $T_{\text{now}}$. This avoids background timer threads entirely.
</details>

<details>
<summary>5. What HTTP status code and headers should be returned when a rate limit is exceeded?</summary>

**Answer:**
- **Status Code**: `HTTP 429 Too Many Requests`.
- **Standard Headers**:
  - `Retry-After`: The number of seconds the client must wait before making another request.
  - `RateLimit-Limit`: Maximum requests permitted in the window.
  - `RateLimit-Remaining`: Number of tokens remaining in the current window.
  - `RateLimit-Reset`: Seconds remaining until full bucket replenishment.
</details>

<details>
<summary>6. How do you defend against distributed credential stuffing where attackers use botnets with rotating IP addresses?</summary>

**Answer:**
IP rate limiting alone is useless against rotating botnets. You must implement **Target Identity Rate Limiting**: track failed attempts per target username or account identifier across all IPs. If account `user@example.com` experiences 5 failed attempts in 5 minutes from any combination of IPs, lock that specific account or trigger mandatory CAPTCHA/MFA challenges.
</details>

<details>
<summary>7. What is IP Tarpitting?</summary>

**Answer:**
IP Tarpitting is a defensive technique that artificially slows down responses to suspicious clients (e.g., delaying failed login responses by 1 to 3 seconds). This consumes attacker bot resources and drastically reduces the throughput of brute-force tools without degrading performance for legitimate users.
</details>

<details>
<summary>8. How do you safely extract the real client IP address when running behind reverse proxies?</summary>

**Answer:**
Inspect the `X-Forwarded-For` header, but **only** trust it if your application server validates that the immediate socket connection originated from a trusted reverse proxy (e.g., Cloudflare IP ranges or AWS ALB private subnets). Using Spring Boot's `server.forward-headers-strategy=framework` ensures proxies are verified before headers are parsed.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Basic Bucket4j Definition (Easy)</summary>

**Task**: Create a Bucket4j `Bucket` that allows a maximum burst of 50 tokens and refills at a rate of 10 tokens per second.

**Solution**:
```java
Bucket bucket = Bucket.builder()
    .addLimit(Bandwidth.classic(50, Refill.greedy(10, Duration.ofSeconds(1))))
    .build();
```
</details>

<details>
<summary>Exercise 2: HTTP 429 Error Response Writer (Easy)</summary>

**Task**: Write a method that sets the HTTP status to 429 and appends a `Retry-After: 30` header on an `HttpServletResponse`.

**Solution**:
```java
public void send429Response(HttpServletResponse response) throws IOException {
    response.setStatus(429);
    response.setHeader("Retry-After", "30");
    response.setContentType("application/json");
    response.getWriter().write("{\"error\": \"Rate limit exceeded. Retry in 30 seconds.\"}");
}
```
</details>

<details>
<summary>Exercise 3: Redis Failed Login Counter (Medium)</summary>

**Task**: Implement a service method that increments a failed login counter for a username in Redis and sets a 10-minute TTL on the first failure.

**Solution**:
```java
public void recordFailedLogin(String username, StringRedisTemplate redis) {
    String key = "login:fails:" + username;
    Long count = redis.opsForValue().increment(key);
    if (count != null && count == 1) {
        redis.expire(key, Duration.ofMinutes(10));
    }
}
```
</details>

<details>
<summary>Exercise 4: IP Extraction Helper (Medium)</summary>

**Task**: Implement a helper method that extracts the original client IP from the `X-Forwarded-For` header, correctly handling comma-separated proxy chains.

**Solution**:
```java
public String extractClientIp(HttpServletRequest request) {
    String xff = request.getHeader("X-Forwarded-For");
    if (xff != null && !xff.isBlank()) {
        return xff.split(",")[0].trim();
    }
    return request.getRemoteAddr();
}
```
</details>

<details>
<summary>Exercise 5: Rate Limiting HandlerInterceptor (Hard)</summary>

**Task**: Implement a Spring MVC `HandlerInterceptor` that checks an `@RateLimited` annotation on controller methods and rejects requests with HTTP 429 when the user's bucket is exhausted.

**Solution**:
```java
@Component
public class RateLimitInterceptor implements HandlerInterceptor {

    private final DistributedRateLimitService rateLimitService;

    public RateLimitInterceptor(DistributedRateLimitService rateLimitService) {
        this.rateLimitService = rateLimitService;
    }

    @Override
    public boolean preHandle(HttpServletRequest request, HttpServletResponse response, Object handler) throws Exception {
        if (handler instanceof HandlerMethod handlerMethod) {
            RateLimited annotation = handlerMethod.getMethodAnnotation(RateLimited.class);
            if (annotation != null) {
                String key = request.getRemoteAddr() + ":" + request.getRequestURI();
                if (!rateLimitService.tryConsume(key, 1)) {
                    response.setStatus(429);
                    response.getWriter().write("{\"error\": \"Rate limit exceeded\"}");
                    return false;
                }
            }
        }
        return true;
    }
}
```
</details>

<details>
<summary>Exercise 6: MockMvc Test for Rate Limiting Filter (Hard)</summary>

**Task**: Write an integration test using MockMvc that sends 6 consecutive requests to a rate-limited endpoint (quota: 5) and asserts that the 6th request fails with status HTTP 429.

**Solution**:
```java
@SpringBootTest
@AutoConfigureMockMvc
class RateLimitingIntegrationTest {

    @Autowired
    private MockMvc mockMvc;

    @Test
    void shouldBlockSixthRequestWith429() throws Exception {
        for (int i = 0; i < 5; i++) {
            mockMvc.perform(get("/api/public/limited-resource"))
                .andExpect(status().isOk());
        }

        mockMvc.perform(get("/api/public/limited-resource"))
            .andExpect(status().isTooManyRequests())
            .andExpect(header().exists("Retry-After"));
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Token Bucket is King**: Bucket4j provides lock-free, burst-friendly rate limiting with zero timer threads.
- **Always Distribute via Redis**: In-memory rate limiting fails across multi-pod Kubernetes clusters.
- **Dual-Key Defense for Logins**: Limit by both IP (to block flood attacks) and username (to block distributed credential stuffing).
- **Standardize Headers**: Emit `RateLimit-*` and `Retry-After` headers on 429 responses.
- **Fail Closed on Auth, Fail Open on Reads**: If Redis is degraded, protect authentication routes while preserving read-only availability.

---

## 13. Further Reading
- [Bucket4j Official Documentation](https://bucket4j.com/)
- [IETF RFC 6585: Additional HTTP Status Codes (429)](https://datatracker.ietf.org/doc/html/rfc6585)
- [IETF Draft: RateLimit Header Fields for HTTP](https://datatracker.ietf.org/doc/html/draft-ietf-httpapi-ratelimit-headers)
- [OWASP Credential Stuffing Prevention Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Credential_Stuffing_Prevention_Cheat_Sheet.html)
- [Redis Rate Limiting Patterns](https://redis.io/glossary/rate-limiting/)
