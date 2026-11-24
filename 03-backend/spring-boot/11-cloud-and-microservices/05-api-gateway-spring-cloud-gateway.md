# Spring Cloud Gateway: Reactive Routing, Rate Limiting, and Security Filters

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | `spring-cloud-starter-gateway:4.2.x`, Netty, Project Reactor |

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
In microservice architectures, exposing dozens of internal microservices directly to external web and mobile clients introduces severe security and architectural liabilities. Clients would need to know internal IP addresses and ports, handle cross-cutting authentication for every service independently, suffer high latency from making multiple fine-grained calls over mobile networks, and deal with disparate CORS configurations. Furthermore, internal APIs would remain unprotected against denial-of-service (DoS) attacks and traffic spikes without centralized rate limiting.

### The Evolution
An **API Gateway** acts as the single, unified entry point for all external client traffic. Historically, Netflix Zuul 1.x provided blocking, servlet-based gateway routing. As concurrency demands grew into hundreds of thousands of concurrent requests, **Spring Cloud Gateway** was built from the ground up using **Project Reactor and Netty**. It operates as a fully non-blocking, reactive edge gateway providing dynamic routing, path rewriting, token relay authentication, and Redis-backed distributed rate limiting.

### Relevance in Modern Engineering
Spring Cloud Gateway is the primary edge routing layer in the Spring Cloud ecosystem. It decouples external public contracts from internal microservice topologies, enforcing centralized TLS termination, OAuth2/OIDC token verification, and automated request throttling at the perimeter.

---

## 2. Core Concepts

- **Route**: The basic building block of the gateway, defined by an ID, a destination URI, a collection of Predicates, and a collection of Filters.
- **Predicate**: A Java 8 `Predicate<ServerWebExchange>` that evaluates incoming HTTP request attributes (headers, paths, query parameters, cookies, HTTP methods) to determine if the route matches.
- **Filter**: A `GatewayFilter` instance that intercepts and modifies the incoming HTTP request or outgoing HTTP response (e.g., adding headers, stripping path prefixes, validating JWTs).
- **GlobalFilter**: A special filter applied conditionally or unconditionally to *all* routes across the gateway (e.g., distributed tracing correlation, global access logging).
- **RequestRateLimiter GatewayFilterFactory**: Built-in filter leveraging the Redis **Token Bucket Algorithm** to throttle requests per user, IP, or API key.
- **Reactive Non-Blocking Architecture**: Runs on Netty event loops without standard Tomcat Servlet threads, achieving extreme concurrency with minimal memory overhead.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        Spring Cloud Gateway Processing Flow                       |
|                                                                                   |
| 1. Client HTTP Request: GET /api/v1/orders/101                                    |
|                   |                                                               |
|                   v                                                               |
| 2. Gateway Handler Mapping                                                        |
|    +---------------------------------------------------------------------+        |
|    | RoutePredicateHandlerMapping evaluates Route Predicates             |        |
|    | Matches: Path("/api/v1/orders/**") -> Route ID: "order_service"     |        |
|    +---------------------------------------------------------------------+        |
|                   |                                                               |
|                   v                                                               |
| 3. Filtering Web Handler Pipeline (Pre-Filters)                                   |
|    +---------------------------------------------------------------------+        |
|    | GlobalAuthFilter ---> StripPrefix(2) ---> RequestRateLimiterFilter  |        |
|    +---------------------------------------------------------------------+        |
|                   | Rewrites URI: /orders/101                                     |
|                   v                                                               |
| 4. Proxied Outbound Request via Netty HttpClient                                  |
|    Forward to Destination: http://order-service:8081/orders/101                   |
|                   |                                                               |
|                   v Target Microservice Processes & Returns 200 OK                |
| 5. Post-Filters Pipeline                                                          |
|    +---------------------------------------------------------------------+        |
|    | AddResponseHeader("X-Response-Time", "12ms") ---> MetricLogger      |        |
|    +---------------------------------------------------------------------+        |
|                   |                                                               |
|                   v                                                               |
| 6. HTTP Response Returned to Client                                               |
+-----------------------------------------------------------------------------------+
```

Incoming requests enter the Netty channel and are handled by `RoutePredicateHandlerMapping`. It iterates through configured routes, testing their `Predicate` definitions against the `ServerWebExchange`. When a match is found, the request is passed to the `FilteringWebHandler`. The handler constructs a filter chain combining global filters and route-specific filters. "Pre"-filter logic executes before forwarding the request via non-blocking Netty `HttpClient`. When the downstream microservice replies, "Post"-filter logic executes on the response before returning it to the client.

---

## 4. Syntax & Usage

### Maven Dependency
```xml
<dependency>
    <groupId>org.springframework.cloud</groupId>
    <artifactId>spring-cloud-starter-gateway</artifactId>
</dependency>
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-data-redis-reactive</artifactId>
</dependency>
```

### Important Warning
**Never include `spring-boot-starter-web`** in a Spring Cloud Gateway application! Gateway is built on Spring WebFlux and Netty; including Tomcat triggers a fatal startup collision.

---

## 5. Practical Examples

### Example 1: Declarative Route Definitions in `application.yml`
```yaml
server:
  port: 8080

spring:
  application:
    name: edge-gateway
  cloud:
    gateway:
      routes:
        - id: order-service-route
          uri: lb://order-service
          predicates:
            - Path=/api/orders/**
            - Method=GET,POST,PUT
          filters:
            - StripPrefix=1 # Rewrites /api/orders/123 to /orders/123
            - AddRequestHeader=X-Gateway-Forwarded, true
            - AddResponseHeader=X-Frame-Options, DENY

        - id: customer-service-route
          uri: lb://customer-service
          predicates:
            - Path=/api/customers/**
          filters:
            - StripPrefix=1
```

### Example 2: Programmatic Java DSL Route Configuration with Fallback
```java
package com.example.platform.gateway.config;

import org.springframework.cloud.gateway.route.RouteLocator;
import org.springframework.cloud.gateway.route.builder.RouteLocatorBuilder;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

@Configuration
public class ProgrammaticGatewayRouteConfig {

    @Bean
    public RouteLocator gatewayRoutes(RouteLocatorBuilder builder) {
        return builder.routes()
                .route("payment_route", r -> r
                        .path("/api/payments/**")
                        .and()
                        .header("X-Client-Type", "mobile")
                        .filters(f -> f
                                .stripPrefix(1)
                                .addResponseHeader("X-Gateway-Route", "mobile-payments")
                                .circuitBreaker(c -> c
                                        .setName("paymentCircuitBreaker")
                                        .setFallbackUri("forward:/fallback/payments")))
                        .uri("lb://payment-service"))
                .build();
    }
}
```

### Example 3: Redis-Backed Distributed Rate Limiting
```java
package com.example.platform.gateway.ratelimit;

import org.springframework.cloud.gateway.filter.ratelimit.KeyResolver;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.context.annotation.Primary;
import reactor.core.publisher.Mono;

import java.security.Principal;

@Configuration
public class RateLimiterConfig {

    // Rate limits by authenticated user identity or falls back to client IP address
    @Bean
    @Primary
    public KeyResolver userKeyResolver() {
        return exchange -> exchange.getPrincipal()
                .map(Principal::getName)
                .switchIfEmpty(Mono.justOrEmpty(
                        exchange.getRequest().getRemoteAddress() != null ?
                                exchange.getRequest().getRemoteAddress().getAddress().getHostAddress() : "anonymous"
                ));
    }
}
```

```yaml
# application.yml: Token bucket rate limiter configuration
spring:
  cloud:
    gateway:
      routes:
        - id: catalog-service-route
          uri: lb://catalog-service
          predicates:
            - Path=/api/products/**
          filters:
            - StripPrefix=1
            - name: RequestRateLimiter
              args:
                redis-rate-limiter.replenishRate: 10 # 10 requests allowed per second
                redis-rate-limiter.burstCapacity: 20  # Burst capacity of 20 tokens
                key-resolver: "#{@userKeyResolver}"
```

### Example 4: The Blocking Servlet Filter Anti-Pattern and Fix
```java
// ANTI-PATTERN: Attempting to use javax.servlet.Filter or blocking JDBC calls inside a Gateway filter!
// public class BadFilter implements Filter { ... } // CRASHES! Gateway runs on Netty, NOT Servlet!
// Synchronously blocking a Netty event loop thread halts thousands of concurrent HTTP connections!

// FIX: Implement GlobalFilter returning Mono<Void> and execute non-blocking operations
package com.example.platform.gateway.filter;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.cloud.gateway.filter.GatewayFilterChain;
import org.springframework.cloud.gateway.filter.GlobalFilter;
import org.springframework.core.Ordered;
import org.springframework.stereotype.Component;
import org.springframework.web.server.ServerWebExchange;
import reactor.core.publisher.Mono;

@Component
public class LoggingGlobalFilter implements GlobalFilter, Ordered {

    private static final Logger log = LoggerFactory.getLogger(LoggingGlobalFilter.class);

    @Override
    public Mono<Void> filter(ServerWebExchange exchange, GatewayFilterChain chain) {
        long startTime = System.currentTimeMillis();
        String path = exchange.getRequest().getURI().getPath();

        // Pre-filter: Log incoming request path
        log.info("GATEWAY INBOUND: [{}] {}", exchange.getRequest().getMethod(), path);

        return chain.filter(exchange).then(Mono.fromRunnable(() -> {
            // Post-filter: Log duration and response status code
            long duration = System.currentTimeMillis() - startTime;
            log.info("GATEWAY OUTBOUND: [{}] {} returned status [{}] in {} ms",
                    exchange.getRequest().getMethod(),
                    path,
                    exchange.getResponse().getStatusCode(),
                    duration);
        }));
    }

    @Override
    public int getOrder() {
        return -1; // High priority in filter chain
    }
}
```

### Example 5: High-Performance Global JWT Authentication and Token Relay Filter
```java
package com.example.platform.gateway.security;

import org.springframework.cloud.gateway.filter.GatewayFilterChain;
import org.springframework.cloud.gateway.filter.GlobalFilter;
import org.springframework.core.Ordered;
import org.springframework.http.HttpHeaders;
import org.springframework.http.HttpStatus;
import org.springframework.stereotype.Component;
import org.springframework.web.server.ServerWebExchange;
import reactor.core.publisher.Mono;

@Component
public class JwtAuthenticationGatewayFilter implements GlobalFilter, Ordered {

    @Override
    public Mono<Void> filter(ServerWebExchange exchange, GatewayFilterChain chain) {
        String path = exchange.getRequest().getURI().getPath();

        // Bypass public health and documentation endpoints
        if (path.startsWith("/public/") || path.contains("/actuator/")) {
            return chain.filter(exchange);
        }

        String authHeader = exchange.getRequest().getHeaders().getFirst(HttpHeaders.AUTHORIZATION);

        if (authHeader == null || !authHeader.startsWith("Bearer ")) {
            exchange.getResponse().setStatusCode(HttpStatus.UNAUTHORIZED);
            return exchange.getResponse().setComplete(); // Abort request immediately at edge
        }

        String token = authHeader.substring(7);

        // Validate token signature and claims reactively
        if (!isTokenValid(token)) {
            exchange.getResponse().setStatusCode(HttpStatus.UNAUTHORIZED);
            return exchange.getResponse().setComplete();
        }

        // Mutate request to inject verified tenant header downstream
        ServerWebExchange mutatedExchange = exchange.mutate()
                .request(r -> r.header("X-Tenant-Id", extractTenantId(token)))
                .build();

        return chain.filter(mutatedExchange);
    }

    private boolean isTokenValid(String token) {
        return !token.isBlank(); // Simulated validation
    }

    private String extractTenantId(String token) {
        return "tenant_alpha";
    }

    @Override
    public int getOrder() {
        return -100; // Execute before routing filters
    }
}
```

---

## 6. Best Practices

1. **Never Block on Netty Event Loop Threads**: Avoid blocking calls (`Thread.sleep()`, synchronous JDBC, blocking HTTP clients); any block freezes an entire Netty event loop, degrading gateway throughput.
2. **Never Add `spring-boot-starter-web`**: Exclude all Tomcat/Servlet dependencies from the gateway POM; keep the classpath pure WebFlux/Netty.
3. **Use Redis Token Bucket for Rate Limiting**: Deploy `RedisRateLimiter` with explicit `replenishRate` and `burstCapacity` to protect downstream services from distributed denial of service.
4. **Always Define Fallback Routes for Circuit Breakers**: When attaching `.circuitBreaker()` to a route, ensure `fallbackUri` routes to a local non-blocking controller returning graceful degradation JSON.
5. **Centralize CORS Management at the Gateway**: Configure global CORS rules in `application.yml` at the gateway level and disable CORS headers on internal microservices to avoid duplicate header conflicts.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Adding `spring-boot-starter-web` to Gateway | Tomcat and Netty conflict, causing application context startup failure | Remove `spring-boot-starter-web`; use only `spring-cloud-starter-gateway`. |
| Blocking Netty event loop threads with sync I/O | Freezes the event loop, causing severe latency spikes for thousands of concurrent requests | Use reactive `Mono`/`Flux` operators or offload to bounded elastic schedulers. |
| Forgetting `StripPrefix` filter | Gateway passes `/api/orders/123` to downstream service expecting `/orders/123` (404 Not Found) | Add `filters: - StripPrefix=1` to remove leading path segments. |
| Duplicate CORS headers | Gateway and microservice both set `Access-Control-Allow-Origin`, causing browser security rejection | Handle CORS exclusively at the Gateway level; disable CORS on downstream services. |
| Missing `KeyResolver` bean with `RequestRateLimiter` | Gateway fails on startup: "No KeyResolver bean found" | Declare a `@Bean KeyResolver` (e.g. resolving user principal or client IP). |
| Using `localhost` instead of service names with `lb://` | Fails to trigger client-side load balancing and service discovery | Use `uri: lb://service-name` for dynamic discovery routing. |

---

## 8. Performance & Security Notes

- **Extreme Concurrency**: Netty event loops handle tens of thousands of idle or concurrent connections using a fixed number of worker threads (typically equal to $2 \times \text{CPU cores}$).
- **Edge Security Termination**: Terminate TLS at the Gateway and inspect incoming payloads for oversized headers or path traversal attempts before traffic enters the internal cluster network.
- **Header Sanitization**: Strip internal tracking headers (`X-Internal-User-Role`, `X-Bypass-Auth`) from incoming external client requests to prevent header injection spoofing.

---

## 9. Comparison / When to Use What

| Feature | Spring Cloud Gateway | Nginx / Envoy Proxy | Netflix Zuul 1.x (Legacy) |
|---|---|---|---|
| **Architecture** | Reactive Netty / WebFlux | C / C++ Event-driven proxy | Blocking Servlet / Tomcat |
| **Custom Java Logic** | Full Java/Spring integration | Complex (Lua / WASM) | Full Java integration |
| **Rate Limiting** | Native Redis Token Bucket | Module-based (limit_req) | Custom filter code |
| **Throughput** | High (50k - 100k req/sec) | Extremely High (150k+ req/sec)| Low / Moderate |
| **Best For** | Spring microservice platforms | Kubernetes Ingress, Edge proxy | Deprecated (Do not use) |

---

## 10. Interview Questions

<details>
<summary>1. How does Spring Cloud Gateway differ architecturally from legacy Netflix Zuul 1.x?</summary>
Netflix Zuul 1.x was built on standard synchronous, blocking Servlet technology (Tomcat). Each incoming request held a dedicated thread for the duration of the upstream and downstream network calls, leading to thread exhaustion under heavy traffic. Spring Cloud Gateway was architected using non-blocking, reactive I/O (Project Reactor and Netty). Requests are multiplexed across a small, fixed pool of event loop threads, delivering significantly higher concurrency, lower memory overhead, and resilience against slow connections.
</details>

<details>
<summary>2. What is the anatomy of a Route in Spring Cloud Gateway?</summary>
A Route consists of:
1. `id`: A unique string identifier.
2. `uri`: The destination address where matching requests are forwarded (e.g., `http://localhost:8081` or `lb://order-service`).
3. `predicates`: Conditions that must evaluate to true for the route to be selected (matching path, headers, methods, query parameters).
4. `filters`: Modifiers that alter the request before sending it downstream, or modify the response before returning it to the client.
</details>

<details>
<summary>3. Why can you NOT include `spring-boot-starter-web` in a Spring Cloud Gateway project?</summary>
Spring Cloud Gateway requires Spring WebFlux and an embedded Netty server to operate its non-blocking event loop architecture. `spring-boot-starter-web` pulls in Spring WebMvc and an embedded Apache Tomcat Servlet container. Having both on the classpath creates an architectural conflict, and Spring Cloud Gateway actively detects and aborts execution to prevent thread blocking.
</details>

<details>
<summary>4. How does the Redis Rate Limiter filter work in Spring Cloud Gateway?</summary>
It implements the **Token Bucket Algorithm** backed by Redis and Lua scripts. For each incoming request, the configured `KeyResolver` extracts an identity key (user ID, IP address). Redis checks whether tokens are available in that key's bucket. If tokens exist, a token is decremented and the request passes; if the bucket is empty, the gateway immediately rejects the request with HTTP 429 Too Many Requests.
</details>

<details>
<summary>5. What is the difference between a GatewayFilter and a GlobalFilter?</summary>
A `GatewayFilter` is applied only to specific routes where it is explicitly configured in properties or Java DSL. A `GlobalFilter` is a Spring component that automatically intercepts *all* requests passing through the gateway, ideal for cross-cutting security, distributed tracing, and global access logging.
</details>

<details>
<summary>6. What does the `StripPrefix` filter do and why is it commonly required?</summary>
Clients typically send requests to the gateway with API prefixes (e.g., `/api/v1/orders/123`). However, the downstream `order-service` controller might be mapped to `/orders/123`. `StripPrefix=1` or `StripPrefix=2` strips the specified number of leading path segments before forwarding the request, avoiding 404 errors downstream.
</details>

<details>
<summary>7. How do you implement a Circuit Breaker fallback route in Spring Cloud Gateway?</summary>
By attaching the `CircuitBreaker` filter to a route: `.circuitBreaker(c -> c.setName("orderCB").setFallbackUri("forward:/fallback/orders"))`. If the downstream microservice times out or fails repeatedly, the circuit opens, and subsequent requests are routed locally to the gateway's `/fallback/orders` controller.
</details>

<details>
<summary>8. How do you handle CORS centrally at the Gateway level?</summary>
By declaring `spring.cloud.gateway.globalcors` in `application.yml`. The configuration defines allowed origins, methods, headers, and credentials. When pre-flight `OPTIONS` requests arrive, the gateway handles them immediately without forwarding to downstream microservices, ensuring consistent CORS headers.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1 (Easy): Configure a basic path-based route in YAML</summary>
Write an `application.yml` route mapping `/customers/**` to `http://localhost:8081` with `StripPrefix=1`.
</details>

<details>
<summary>Exercise 2 (Easy): Add an outbound custom response header</summary>
Add a filter to a route that injects header `X-Gateway-Processed: true` into the client HTTP response.
</details>

<details>
<summary>Exercise 3 (Medium): Implement an IP-based KeyResolver</summary>
Write a Spring `@Bean` implementing `KeyResolver` that extracts the client IP address from `ServerWebExchange`.
</details>

<details>
<summary>Exercise 4 (Medium): Build a global request duration logging filter</summary>
Create a `GlobalFilter` that records request start time and logs the execution duration upon response completion.
</details>

<details>
<summary>Exercise 5 (Hard): Token validation and header mutation filter</summary>
Implement a `GatewayFilter` that extracts an API key from header `X-API-KEY`, validates it, and injects `X-User-Role: ADMIN` before forwarding downstream.
</details>

<details>
<summary>Exercise 6 (Hard): Integration test testing gateway routing and rate limiting</summary>
Write a `@SpringBootTest(webEnvironment = RANDOM_PORT)` test using `WebTestClient` that issues 15 rapid requests and asserts that requests beyond burst capacity return HTTP 429.
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                       Spring Cloud Gateway Summary                              |
+---------------------------------------------------------------------------------+
| Dependency:         spring-cloud-starter-gateway (Netty/Reactive only!)         |
| Invariant:          NEVER include spring-boot-starter-web                       |
| Route Structure:    ID + URI (lb://service) + Predicates + Filters              |
| Path Rewriting:     filters: - StripPrefix=1 (or RewritePath)                   |
| Rate Limiting:      RequestRateLimiter with RedisRateLimiter + KeyResolver      |
| Global Filters:     GlobalFilter returning Mono<Void> (Zero blocking code)      |
| Dynamic Discovery:  uri: lb://order-service (uses Spring Cloud LoadBalancer)    |
| Fallback:           circuitBreaker(c -> c.setFallbackUri("forward:/fallback"))  |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Official Spring Cloud Gateway Reference](https://docs.spring.io/spring-cloud-gateway/reference/)
- [Project Reactor Official Reference](https://projectreactor.io/docs/core/release/reference/)
- [Token Bucket Algorithm Explanation](https://en.wikipedia.org/wiki/Token_bucket)
