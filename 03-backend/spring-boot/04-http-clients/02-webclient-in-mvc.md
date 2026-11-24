# WebClient in Spring MVC Applications

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | Spring MVC, Reactive Streams Basics, Reactor Netty |
| **Estimated Reading Time** | 22 minutes |
| **Status** | Production-Ready |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | OpenJDK 21 LTS / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | Reactor Netty 1.2, OpenJDK 21.0.2 |

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
9. [Comparison: WebClient in MVC vs WebFlux Application](#9-comparison-webclient-in-mvc-vs-webflux-application)
10. [Interview Questions & Deep Dives](#10-interview-questions--deep-dives)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Before Spring Boot 3.2 introduced `RestClient`, `WebClient` was the only modern fluent HTTP client available in the Spring ecosystem. Even in standard Servlet-based Spring MVC applications, many enterprise systems adopted `WebClient` for high-throughput streaming, parallel non-blocking external API calls, or WebSocket interactions.

### The Problem
Using a reactive client (`WebClient`) inside a synchronous, blocking Servlet container (Tomcat) creates architectural tension:
1. Calling `.block()` on worker threads can lead to thread starvation if not carefully managed.
2. If executed on Netty event-loop threads, calling `.block()` throws `IllegalStateException: block()/blockFirst()/blockLast() are blocking, which is not supported on thread reactor-http-nio-*`.
3. Memory buffer limits in WebClient default to 256KB, causing unexpected `DataBufferLimitException` on large responses.

### The Solution: Idiomatic WebClient Integration in Spring MVC
Spring Boot supports using `WebClient` seamlessly inside Spring MVC. By configuring Reactor Netty connection pools, tuning codecs, and properly bridging reactive publishers (`Mono`, `Flux`) to synchronous controller responses or asynchronous `DeferredResult` wrappers, teams can leverage WebClient's streaming and connection reuse capabilities safely.

---

## 2. Core Concepts

### Mental Model & Execution Boundary
```
           Incoming HTTP Request (Tomcat Worker Thread #1)
                                │
                                ▼
                   Spring MVC Controller Method
                                │
                                ▼
           WebClient.get().uri("/data").retrieve().bodyToMono(Dto.class)
                                │
                                ▼
           Dispatches over Reactor Netty Event Loop Thread Pool
           (Non-blocking I/O over shared TCP connection pool)
                                │
           ┌────────────────────┴────────────────────┐
           ▼                                         ▼
   Option A: Synchronous Bridge              Option B: Async Streaming Bridge
   .block(Duration.ofSeconds(5))             Returns Mono<Dto> or DeferredResult
   Tomcat thread waits safely                Tomcat thread released immediately
   Returns Dto directly                      Dispatches result when Mono completes
```

### Invariants
1. **Never Block on Event Loop Threads**: You can safely call `.block()` on a Tomcat worker thread (`http-nio-*`) or a Virtual Thread, but you must **NEVER** call `.block()` inside a reactive operator callback running on a Reactor Netty event-loop thread.
2. **Buffer Limits**: The default in-memory buffer limit for codecs is 256KB. Large payloads will fail with `DataBufferLimitException` unless `maxInMemorySize` is explicitly tuned.
3. **Connection Pooling**: WebClient uses an underlying Reactor Netty `ConnectionProvider` managing pooled TCP sockets with keep-alive and idle eviction.

---

## 3. How It Works Internally

### The Reactor Netty HTTP Connector
`WebClient` delegates wire protocol communication to `ClientHttpConnector` (default `ReactorClientHttpConnector`):
1. **Event Loop Groups**: Uses Netty's `NioEventLoopGroup` or Epoll (on Linux) to handle socket polling asynchronously.
2. **ConnectionProvider**: Manages pools of active connections (default 500 max connections per pool, 1000 pending acquires).
3. **Reactive Pipelines**: Requests and responses are handled as `Publisher<DataBuffer>`, streaming bytes asynchronously without buffering the entire body into memory unless `.bodyToMono()` is called.

---

## 4. Syntax & Configuration

### Dependency Setup (`pom.xml`)
```xml
<!-- Required to use WebClient in an MVC application -->
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-webflux</artifactId>
</dependency>
```

### Production WebClient Configuration Bean
```java
package com.example.clients.config;

import io.netty.channel.ChannelOption;
import io.netty.handler.timeout.ReadTimeoutHandler;
import io.netty.handler.timeout.WriteTimeoutHandler;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.http.client.reactive.ReactorClientHttpConnector;
import org.springframework.web.reactive.function.client.ExchangeStrategies;
import org.springframework.web.reactive.function.client.WebClient;
import reactor.netty.http.client.HttpClient;
import reactor.netty.resources.ConnectionProvider;

import java.time.Duration;
import java.util.concurrent.TimeUnit;

@Configuration(proxyBeanMethods = false)
public class WebClientMvcConfig {

    @Bean
    public WebClient externalApiWebClient(WebClient.Builder builder) {
        // High-scale connection pool
        ConnectionProvider connectionProvider = ConnectionProvider.builder("custom-pool")
                .maxConnections(500)
                .maxIdleTime(Duration.ofSeconds(20))
                .maxLifeTime(Duration.ofMinutes(5))
                .pendingAcquireTimeout(Duration.ofSeconds(10))
                .evictInBackground(Duration.ofSeconds(30))
                .build();

        HttpClient httpClient = HttpClient.create(connectionProvider)
                .option(ChannelOption.CONNECT_TIMEOUT_MILLIS, 5000)
                .responseTimeout(Duration.ofSeconds(10))
                .doOnConnected(conn -> conn
                        .addHandlerLast(new ReadTimeoutHandler(10, TimeUnit.SECONDS))
                        .addHandlerLast(new WriteTimeoutHandler(10, TimeUnit.SECONDS)));

        // Increase buffer memory size from 256KB to 10MB
        ExchangeStrategies strategies = ExchangeStrategies.builder()
                .codecs(configurer -> configurer.defaultCodecs().maxInMemorySize(10 * 1024 * 1024))
                .build();

        return builder
                .clientConnector(new ReactorClientHttpConnector(httpClient))
                .exchangeStrategies(strategies)
                .baseUrl("https://partner-service.internal")
                .defaultHeader("Accept", "application/json")
                .build();
    }
}
```

---

## 5. Practical Examples

### Example 1: Controlled Blocking Call in a Spring MVC Controller

```java
package com.example.clients.controller;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;
import org.springframework.web.reactive.function.client.WebClient;

import java.time.Duration;
import java.util.UUID;

public record UserProfileDto(UUID id, String username, String email) {}

@RestController
@RequestMapping("/api/v1/profiles")
public class UserProfileProxyController {

    private static final Logger log = LoggerFactory.getLogger(UserProfileProxyController.class);
    private final WebClient webClient;

    public UserProfileProxyController(WebClient webClient) {
        this.webClient = webClient;
    }

    @GetMapping("/{id}")
    public ResponseEntity<UserProfileDto> getUserProfile(@PathVariable UUID id) {
        log.info("Handling request on Tomcat worker thread: {}", Thread.currentThread().getName());

        // Calling .block() on Tomcat thread with an explicit, bounded timeout!
        UserProfileDto profile = webClient.get()
                .uri("/users/{id}", id)
                .retrieve()
                .bodyToMono(UserProfileDto.class)
                .block(Duration.ofSeconds(5)); // NEVER call .block() without an explicit timeout!

        return ResponseEntity.ok(profile);
    }
}
```

### Example 2: Non-Blocking Reactive Aggregation with `Mono.zip` in Spring MVC

When an endpoint needs to call two external services simultaneously, `Mono.zip` executes them concurrently without wasting thread time.

```java
package com.example.clients.service;

import org.springframework.stereotype.Service;
import org.springframework.web.reactive.function.client.WebClient;
import reactor.core.publisher.Mono;

import java.time.Duration;
import java.util.UUID;

public record AccountSummary(String creditScore, String balance) {}

@Service
public class AggregationClientService {

    private final WebClient webClient;

    public AggregationClientService(WebClient webClient) {
        this.webClient = webClient;
    }

    public AccountSummary fetchAccountSummaryConcurrently(UUID accountId) {
        Mono<String> creditScoreMono = webClient.get()
                .uri("/credit-service/scores/{id}", accountId)
                .retrieve()
                .bodyToMono(String.class);

        Mono<String> balanceMono = webClient.get()
                .uri("/ledger-service/balances/{id}", accountId)
                .retrieve()
                .bodyToMono(String.class);

        // Executes both calls in parallel on Reactor Netty event loops!
        return Mono.zip(creditScoreMono, balanceMono)
                .map(tuple -> new AccountSummary(tuple.getT1(), tuple.getT2()))
                .block(Duration.ofSeconds(6)); // Blocks only for max(timeA, timeB)
    }
}
```

### Example 3: Edge Case — Handling `DataBufferLimitException` on Large Payloads

```java
package com.example.clients.codec;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.web.reactive.function.client.ExchangeStrategies;
import org.springframework.web.reactive.function.client.WebClient;

@Configuration(proxyBeanMethods = false)
public class LargePayloadWebClientConfig {

    /**
     * Prevents: org.springframework.core.io.buffer.DataBufferLimitException:
     * Exceeded limit on max bytes to buffer : 262144
     */
    @Bean
    public WebClient reportingWebClient(WebClient.Builder builder) {
        ExchangeStrategies strategies = ExchangeStrategies.builder()
                .codecs(codecs -> codecs.defaultCodecs().maxInMemorySize(50 * 1024 * 1024)) // 50MB
                .build();

        return builder
                .baseUrl("https://reporting-engine.internal")
                .exchangeStrategies(strategies)
                .build();
    }
}
```

### Example 4: Anti-Pattern vs Production Fix

#### Anti-Pattern: Calling `.block()` inside a Reactive Operator
```java
// ANTI-PATTERN: Blocking inside flatMap running on Netty event-loop thread!
webClient.get().uri("/users")
    .retrieve()
    .bodyToFlux(User.class)
    .flatMap(user -> {
        // FATAL ERROR: Throws IllegalStateException: block() is not supported on thread reactor-http-nio-*
        UserPermissions perms = permissionClient.get().uri("/perms/" + user.id())
            .retrieve()
            .bodyToMono(UserPermissions.class)
            .block(); 
        return Mono.just(new EnrichedUser(user, perms));
    });
```

#### Production Fix: Use Reactive Composition (`flatMap`)
```java
// PRODUCTION FIX: Chain reactive publishers asynchronously without blocking
webClient.get().uri("/users")
    .retrieve()
    .bodyToFlux(User.class)
    .flatMap(user -> permissionClient.get().uri("/perms/" + user.id())
        .retrieve()
        .bodyToMono(UserPermissions.class)
        .map(perms -> new EnrichedUser(user, perms))
    );
```

### Example 5: High-Performance Server-Sent Events (SSE) Client Consumer

```java
package com.example.clients.sse;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.core.ParameterizedTypeReference;
import org.springframework.http.codec.ServerSentEvent;
import org.springframework.stereotype.Service;
import org.springframework.web.reactive.function.client.WebClient;
import reactor.core.Disposable;

@Service
public class RealtimeMarketDataConsumer {

    private static final Logger log = LoggerFactory.getLogger(RealtimeMarketDataConsumer.class);
    private final WebClient webClient;

    public RealtimeMarketDataConsumer(WebClient webClient) {
        this.webClient = webClient;
    }

    public Disposable subscribeToMarketStream() {
        ParameterizedTypeReference<ServerSentEvent<String>> type = new ParameterizedTypeReference<>() {};

        return webClient.get()
                .uri("/market-events")
                .retrieve()
                .bodyToFlux(type)
                .subscribe(
                        event -> log.info("Received SSE Event: [ID={}] Data={}", event.id(), event.data()),
                        error -> log.error("Error in SSE stream", error),
                        () -> log.info("SSE Stream completed.")
                );
    }
}
```

---

## 6. Best Practices

1. **Always Supply Timeouts to `.block()`**: Never call bare `.block()`. Always pass an explicit duration (`.block(Duration.ofSeconds(5))`) to avoid thread hangs.
2. **Increase `maxInMemorySize`**: The default 256KB codec limit is too small for modern JSON payloads; increase it to 10MB or 20MB.
3. **Tune Reactor Netty Connection Provider**: Set `maxConnections`, `maxIdleTime`, and `pendingAcquireTimeout` to prevent resource leaks under burst traffic.
4. **Prefer `RestClient` for New Synchronous MVC Code**: In Spring Boot 3.2+, if you only need synchronous calls without reactive streaming, prefer `RestClient` over `WebClient` to eliminate WebFlux dependencies.
5. **Log Network Metrics via Micrometer**: Ensure Netty metrics are exposed to Micrometer via `httpClient.metrics(true, Function.identity())`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **`DataBufferLimitException: Exceeded limit 262144`** | Response payload exceeds the default 256KB codec memory limit. | Configure `ExchangeStrategies.builder().codecs(c -> c.defaultCodecs().maxInMemorySize(...))`. |
| **Blocking on Netty Event Loop Thread** | Calling `.block()` inside a reactive operator throws `IllegalStateException`. | Chain operations using `flatMap()`; never block on event-loop threads. |
| **Calling `.block()` without Timeout** | If downstream service hangs, Tomcat worker threads leak and freeze indefinitely. | Always use `.block(Duration.ofSeconds(timeout))`. |
| **Creating `new WebClient` per Request** | Spawns thousands of Netty event loop threads and socket pools, crashing the JVM. | Maintain a single shared `WebClient` singleton bean. |
| **Ignoring DNS Refresh in Long-Lived Pools** | Netty caches DNS lookups forever by default, failing during Kubernetes pod IP shifts. | Configure Netty `DnsAddressResolverGroup` with TTL eviction. |
| **Forgetting to Subscribe** | Calling reactive methods without `.subscribe()` or `.block()` does nothing; requests are never sent. | Remember that reactive publishers do nothing until subscribed to. |

---

## 8. Performance & Security Considerations

### Netty Direct Memory Leaks
Reactor Netty utilizes Netty ByteBuf pools allocating off-heap direct memory. Unhandled exceptions or improperly discarded streaming responses can leak ByteBuf buffers, causing `OutOfMemoryError: Direct buffer memory`. Always ensure streaming fluxes are consumed or cancelled cleanly.

### DNS Resolution Tuning in Kubernetes
In Kubernetes environments, pod IP addresses change frequently. By default, JVM or Netty DNS resolvers may cache DNS lookups indefinitely. Configure Reactor Netty with:
```java
httpClient.resolver(DefaultAddressResolverGroup.INSTANCE);
```
or set JVM system property `-Dsun.net.inetaddr.ttl=30`.

---

## 9. Comparison: WebClient in MVC vs WebFlux Application

| Aspect | WebClient in Spring MVC (Servlet) | WebClient in Spring WebFlux (Reactive) |
| :--- | :--- | :--- |
| **Hosting Container** | Embedded Tomcat / Jetty (Servlet 6.0) | Reactor Netty (Event-Driven) |
| **Calling Paradigm** | Mixed: `.block(timeout)` or `DeferredResult` | Pure non-blocking: Return `Mono<T>` / `Flux<T>` |
| **Execution Threads** | Invoked from Tomcat thread (`http-nio-*`) | Invoked from Netty thread (`reactor-http-*`) |
| **Blocking Permitted?**| Yes (on Tomcat worker thread) | NO (Never block on event loops) |
| **Primary Motivation** | Concurrent `Mono.zip`, SSE consumption | End-to-end reactive streaming |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: Why does calling `.block()` on a Reactor Netty event loop thread throw an exception, but works on a Tomcat worker thread?</strong></summary>

**Answer:**
Project Reactor uses BlockHound to detect blocking calls. A Tomcat worker thread is designed for blocking Servlet I/O; blocking it only delays that individual request. However, a Reactor Netty event loop thread handles hundreds of concurrent non-blocking socket connections simultaneously. Blocking an event loop thread freezes all other connections multiplexed onto that thread, causing severe system-wide latency spikes. Hence, Reactor strictly forbids blocking on its event loop threads.
</details>

<details>
<summary><strong>Q2: What is the cause of `DataBufferLimitException` and how do you resolve it?</strong></summary>

**Answer:**
Spring WebClient enforces a default limit of 256KB (`262,144 bytes`) on responses buffered into memory via `HttpMessageReader` to prevent malicious denial-of-service memory exhaustion. If a response exceeds 256KB, it throws `DataBufferLimitException`. To fix it, configure `ExchangeStrategies` with a larger `maxInMemorySize` (e.g. `10 * 1024 * 1024` for 10MB).
</details>

<details>
<summary><strong>Q3: How do you perform two external HTTP calls in parallel using WebClient in a Spring MVC controller?</strong></summary>

**Answer:**
By combining the calls with `Mono.zip()`:
```java
Mono<TypeA> callA = webClient.get().uri("/serviceA").retrieve().bodyToMono(TypeA.class);
Mono<TypeB> callB = webClient.get().uri("/serviceB").retrieve().bodyToMono(TypeB.class);

Tuple2<TypeA, TypeB> result = Mono.zip(callA, callB).block(Duration.ofSeconds(5));
```
Both HTTP requests are dispatched concurrently over the wire, and the block only waits for the slower of the two calls.
</details>

<details>
<summary><strong>Q4: Why is it recommended to migrate from `WebClient` to `RestClient` for synchronous calls in Spring Boot 3.2+?</strong></summary>

**Answer:**
`WebClient` requires the `spring-boot-starter-webflux` dependency, which brings in Netty and Project Reactor. This introduces unnecessary complexity and memory overhead if the application only performs synchronous calls. `RestClient` provides the same modern, fluent API, but belongs to `spring-web`, requires no reactive dependencies, and runs natively on Java 21 Virtual Threads.
</details>

<details>
<summary><strong>Q5: What is the purpose of `ExchangeFilterFunction` in WebClient?</strong></summary>

**Answer:**
`ExchangeFilterFunction` intercepts outgoing requests and incoming responses. It is used to implement cross-cutting concerns like logging, metric recording, dynamic header injection (OAuth2 bearer tokens), and request rewriting across all calls made by the client.
</details>

<details>
<summary><strong>Q6: How does WebClient's connection pooling behave under high load?</strong></summary>

**Answer:**
`ReactorClientHttpConnector` uses Reactor Netty's `ConnectionProvider`. When all connections (e.g. 500) are busy, subsequent requests wait in an acquire queue up to `pendingAcquireTimeout`. If a connection cannot be acquired within the timeout, it throws `io.netty.channel.ConnectTimeoutException`.
</details>

<details>
<summary><strong>Q7: Can a Spring MVC controller method return a `Mono<T>` or `Flux<T>` directly?</strong></summary>

**Answer:**
Yes. Spring MVC supports reactive return types. When a controller returns `Mono<T>`, Spring MVC integrates with Servlet 3.0 asynchronous processing, holding the connection open without occupying a Tomcat worker thread until the `Mono` emits its value.
</details>

<details>
<summary><strong>Q8: What happens if an error occurs during WebClient execution?</strong></summary>

**Answer:**
By default, 4xx responses throw `WebClientResponseException` with specific subclasses like `WebClientResponseException.NotFound` or `WebClientResponseException.BadRequest`. You can customize error handling using `.onStatus(predicate, function)`.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Basic WebClient GET Call with Bounded Block</strong></summary>

**Task**: Write a method that uses `WebClient` to retrieve an item from `/items/{id}` with a 3-second timeout.

```java
// Solution:
public ItemDto fetchItem(Long id) {
    return webClient.get()
            .uri("/items/{id}", id)
            .retrieve()
            .bodyToMono(ItemDto.class)
            .block(Duration.ofSeconds(3));
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Increase Max In-Memory Buffer Size</strong></summary>

**Task**: Configure `WebClient.Builder` with a 5MB buffer limit.

```java
// Solution:
@Bean
public WebClient customBufferWebClient(WebClient.Builder builder) {
    return builder
            .exchangeStrategies(ExchangeStrategies.builder()
                    .codecs(c -> c.defaultCodecs().maxInMemorySize(5 * 1024 * 1024))
                    .build())
            .build();
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Parallel WebClient Calls with Fallback</strong></summary>

**Task**: Use `Mono.zip` to fetch User and Account details concurrently, returning a default empty Account if the account service fails.

```java
// Solution:
public UserDashboard fetchDashboard(UUID userId) {
    Mono<User> userMono = webClient.get().uri("/users/{id}", userId).retrieve().bodyToMono(User.class);
    Mono<Account> accountMono = webClient.get().uri("/accounts/{id}", userId).retrieve().bodyToMono(Account.class)
            .onErrorReturn(new Account("DEFAULT", 0.0));

    return Mono.zip(userMono, accountMono)
            .map(t -> new UserDashboard(t.getT1(), t.getT2()))
            .block(Duration.ofSeconds(4));
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Custom ExchangeFilterFunction Logging</strong></summary>

**Task**: Implement an `ExchangeFilterFunction` that logs the request method and URL before sending.

```java
// Solution:
ExchangeFilterFunction logRequest() {
    return ExchangeFilterFunction.ofRequestProcessor(clientRequest -> {
        System.out.println("WebClient Request: " + clientRequest.method() + " " + clientRequest.url());
        return Mono.just(clientRequest);
    });
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Resilient WebClient with Exponential Retry</strong></summary>

**Task**: Configure a WebClient call with `retryWhen(Retry.backoff(3, Duration.ofMillis(200)))` that retries only on 5xx server errors and network disconnects.
</details>

<details>
<summary><strong>Exercise 6 (Hard): Custom Reactor Netty Mutual TLS WebClient</strong></summary>

**Task**: Build a `WebClient` bean configured with Reactor Netty `SslContextBuilder` establishing mutual TLS (mTLS) with client certificate and private key authentication.
</details>

---

## 12. Summary & Quick Reference

- **Key Role**: Non-blocking reactive client suitable for parallel calls and streaming.
- **Rule of Thumb**: Always pass timeouts to `.block(Duration.ofSeconds(...))`.
- **Never Block on Netty**: Never invoke `.block()` inside reactive operator chains.
- **Buffer Fix**: Increase `maxInMemorySize` beyond the default 256KB.
- **Modern Alternative**: Use `RestClient` for synchronous tasks in Spring Boot 3.2+.

---

## 13. Further Reading

- [Spring Framework Reference: WebClient](https://docs.spring.io/spring-framework/reference/web/webflux-webclient.html)
- [Project Reactor Core Documentation](https://projectreactor.io/docs/core/release/reference/)
- [Reactor Netty Reference Guide](https://projectreactor.io/docs/netty/release/reference/index.html)
- [BlockHound: Java Thread Blocking Detection](https://github.com/reactor/BlockHound)
