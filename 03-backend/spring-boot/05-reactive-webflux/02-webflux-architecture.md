# Spring WebFlux Architecture & Internal Mechanics

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | Reactive Programming with Project Reactor, Netty Basics, Servlet 6.0 vs Reactive |
| **Estimated Reading Time** | 24 minutes |
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
9. [Comparison: Spring MVC vs Spring WebFlux Architecture](#9-comparison-spring-mvc-vs-spring-webflux-architecture)
10. [Interview Questions & Deep Dives](#10-interview-questions--deep-dives)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Spring Framework 5.0 introduced **Spring WebFlux** as an asynchronous, non-blocking parallel stack to the traditional Servlet-based Spring MVC.

### The Problem
Traditional Spring MVC relies on the Jakarta Servlet specification (`HttpServletRequest`, `HttpServletResponse`) and a thread-per-request model. Under extreme concurrency (e.g. 50,000 idle SSE streaming connections or WebSocket microservices), the operating system thread scheduler incurs heavy memory overhead and high CPU context-switching costs. Furthermore, relational drivers and blocking APIs lacked end-to-end backpressure.

### The Solution: Spring WebFlux
Spring WebFlux runs on non-blocking servers (Reactor Netty by default, Undertow, or Servlet 3.1+ containers) using a fixed number of event-loop threads (typically matching the host CPU core count). It eliminates the Servlet API dependency, replacing `DispatcherServlet` with the non-blocking `DispatcherHandler` and abstracting HTTP requests into reactive `ServerWebExchange` representations.

---

## 2. Core Concepts

### Mental Model & Architectural Pipeline
```
               Incoming TCP Traffic (Reactor Netty Channel)
                                    │
                                    ▼
                          HttpHandler Adapter
                     (Translates Netty request/response
                     into Spring ServerHttpRequest/Response)
                                    │
                                    ▼
                         WebFilter Chain (AOP/Security)
                                    │
                                    ▼
                        DispatcherHandler (Core Engine)
                                    │
           ┌────────────────────────┼────────────────────────┐
           ▼                        ▼                        ▼
     HandlerMapping           HandlerAdapter        HandlerResultHandler
  (RequestMapping / Router) (InvocableHandlerMethod) (Writes Mono/Flux to wire)
           │                        │                        │
           └────────────────────────┴────────────────────────┘
                                    │
                                    ▼
                         ServerWebExchange Flushed
                     (Non-blocking stream writes to socket)
```

### Invariants
1. **Event Loop Non-Blocking Invariant**: WebFlux executes on a small, fixed pool of event-loop worker threads. Any blocking operation (`Thread.sleep()`, JDBC, legacy blocking file I/O) freezes an entire event-loop, stalling thousands of concurrent requests.
2. **`ServerWebExchange` Centrality**: The `ServerWebExchange` acts as the unified contract container, holding both `ServerHttpRequest` and `ServerHttpResponse` alongside session attributes and reactive `Context`.
3. **End-to-End Streaming**: Responses are not buffered into memory by default. Streaming endpoints emit chunked `DataBuffer` elements as soon as they are produced by upstream reactive publishers.

---

## 3. How It Works Internally

### The `DispatcherHandler` Execution Flow
`DispatcherHandler` implements `WebHandler`:
1. **`HandlerMapping` Lookup**: Queries registered `HandlerMapping` beans (e.g., `RequestMappingHandlerMapping` or `RouterFunctionMapping`) to find a matching handler, returning `Mono<Object>`.
2. **`HandlerAdapter` Execution**: Dispatches execution to a matching `HandlerAdapter` (e.g., `RequestMappingHandlerAdapter` or `HandlerFunctionAdapter`). The adapter invokes the controller or handler function, returning a `Mono<HandlerResult>`.
3. **`HandlerResultHandler` Rendering**: The result is passed to a `HandlerResultHandler` (e.g., `ResponseBodyResultHandler` for `@ResponseBody` or `ViewResolutionResultHandler`).
4. **Reactive Write**: `ResponseBodyResultHandler` uses reactive `HttpMessageWriter` to serialize the publisher's elements into `DataBuffer` chunks, streaming them to the Netty channel.

---

## 4. Syntax & Configuration

### Dependency Setup (`pom.xml`)
```xml
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-webflux</artifactId>
</dependency>
```

### Standard `application.yaml` WebFlux Server Configuration
```yaml
server:
  port: 8080
  netty:
    connection-timeout: 10000ms
    idle-timeout: 30000ms
spring:
  webflux:
    base-path: /api/v1
```

---

## 5. Practical Examples

### Example 1: Reactive Controller Returning `Mono` and `Flux`

```java
package com.example.webflux.controller;

import org.springframework.http.HttpStatus;
import org.springframework.web.bind.annotation.*;
import reactor.core.publisher.Flux;
import reactor.core.publisher.Mono;

import java.time.Duration;
import java.util.UUID;

public record DeviceTelemetry(UUID deviceId, double temperature, long timestamp) {}

@RestController
@RequestMapping("/api/v1/telemetry")
public class TelemetryController {

    @GetMapping("/{id}")
    public Mono<DeviceTelemetry> getLatestTelemetry(@PathVariable UUID id) {
        // Non-blocking single emission
        return Mono.just(new DeviceTelemetry(id, 24.5, System.currentTimeMillis()));
    }

    @GetMapping
    public Flux<DeviceTelemetry> getAllDevices() {
        // Non-blocking stream emission
        return Flux.range(1, 10)
                .map(i -> new DeviceTelemetry(UUID.randomUUID(), 20.0 + i, System.currentTimeMillis()));
    }
}
```

### Example 2: Custom Reactive `WebFilter` for Distributed Tracing & Security

```java
package com.example.webflux.filter;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.core.Ordered;
import org.springframework.core.annotation.Order;
import org.springframework.stereotype.Component;
import org.springframework.web.server.ServerWebExchange;
import org.springframework.web.server.WebFilter;
import org.springframework.web.server.WebFilterChain;
import reactor.core.publisher.Mono;

import java.util.UUID;

@Component
@Order(Ordered.HIGHEST_PRECEDENCE)
public class ReactiveCorrelationIdFilter implements WebFilter {

    private static final Logger log = LoggerFactory.getLogger(ReactiveCorrelationIdFilter.class);
    public static final String CORRELATION_HEADER = "X-Correlation-ID";

    @Override
    public Mono<Void> filter(ServerWebExchange exchange, WebFilterChain chain) {
        String correlationId = exchange.getRequest().getHeaders().getFirst(CORRELATION_HEADER);
        if (correlationId == null || correlationId.isBlank()) {
            correlationId = UUID.randomUUID().toString();
        }

        // Add correlation header to the outgoing reactive response
        exchange.getResponse().getHeaders().set(CORRELATION_HEADER, correlationId);

        final String activeId = correlationId;
        return chain.filter(exchange)
                // Write correlationId into Reactor Context so downstream operators can access it!
                .contextWrite(context -> context.put("correlationId", activeId))
                .doOnSuccess(v -> log.info("Completed reactive request {} with status {}",
                        activeId, exchange.getResponse().getStatusCode()));
    }
}
```

### Example 3: Edge Case — Server-Sent Events (SSE) Streaming Flux

```java
package com.example.webflux.controller;

import org.springframework.http.MediaType;
import org.springframework.http.codec.ServerSentEvent;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;
import reactor.core.publisher.Flux;

import java.time.Duration;
import java.time.LocalTime;

@RestController
@RequestMapping("/api/v1/sse")
public class ReactiveSseStreamController {

    /**
     * Streams continuous events to the client. The event-loop handles thousands of these
     * streams concurrently at near-zero CPU and memory cost!
     */
    @GetMapping(value = "/clock", produces = MediaType.TEXT_EVENT_STREAM_VALUE)
    public Flux<ServerSentEvent<String>> streamServerClock() {
        return Flux.interval(Duration.ofSeconds(1))
                .map(sequence -> ServerSentEvent.<String>builder()
                        .id(String.valueOf(sequence))
                        .event("CLOCK_TICK")
                        .data("Current server time: " + LocalTime.now())
                        .build());
    }
}
```

### Example 4: Anti-Pattern vs Production Fix

#### Anti-Pattern: Using Blocking JDBC / JPA Inside WebFlux
```java
// ANTI-PATTERN: Invoking blocking Hibernate / JDBC inside WebFlux controller!
@GetMapping("/users/{id}")
public Mono<User> getUserBad(@PathVariable Long id) {
    // FATAL: userRepository.findById() blocks the single Netty event-loop thread!
    // Freezes all other concurrent users sharing this event loop!
    User user = legacyJpaUserRepository.findById(id).orElse(null); 
    return Mono.justOrEmpty(user);
}
```

#### Production Fix: Use Reactive R2DBC Driver or Bounded Elastic Offloading
```java
// PRODUCTION FIX Option A: Use non-blocking R2DBC repository
@GetMapping("/users/{id}")
public Mono<User> getUserGood(@PathVariable Long id) {
    return r2dbcUserRepository.findById(id); // Pure non-blocking reactive driver
}

// PRODUCTION FIX Option B: If blocking JDBC is unavoidable, offload to boundedElastic
@GetMapping("/legacy-users/{id}")
public Mono<User> getLegacyUserGood(@PathVariable Long id) {
    return Mono.fromCallable(() -> legacyJpaUserRepository.findById(id).orElse(null))
               .subscribeOn(Schedulers.boundedElastic());
}
```

### Example 5: High-Performance Request Body Streaming Validation

```java
package com.example.webflux.streaming;

import org.springframework.core.io.buffer.DataBuffer;
import org.springframework.core.io.buffer.DataBufferUtils;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;
import org.springframework.web.server.ServerWebExchange;
import reactor.core.publisher.Mono;

import java.util.concurrent.atomic.AtomicLong;

@RestController
@RequestMapping("/api/v1/stream-upload")
public class ReactiveBodyStreamController {

    /**
     * Processes uploaded bytes iteratively without loading the payload into memory.
     */
    @PostMapping
    public Mono<ResponseEntity<String>> processStream(ServerWebExchange exchange) {
        AtomicLong byteCounter = new AtomicLong();

        return exchange.getRequest().getBody()
                .doOnNext(dataBuffer -> {
                    byteCounter.addAndGet(dataBuffer.readableByteCount());
                    DataBufferUtils.release(dataBuffer); // Crucial: Release Netty buffer to prevent leaks!
                })
                .then(Mono.defer(() -> {
                    return Mono.just(ResponseEntity.status(HttpStatus.CREATED)
                            .body("Successfully streamed " + byteCounter.get() + " bytes through pipeline."));
                }));
    }
}
```

---

## 6. Best Practices

1. **Verify No Blocking Dependencies**: Audit all libraries using **BlockHound**. A single blocking call (JDBC, logging frameworks with synchronous disk appenders) degrades the entire event loop.
2. **Use R2DBC for Relational Databases**: Pair WebFlux with Spring Data R2DBC (`io.r2dbc`) instead of Spring Data JPA/Hibernate.
3. **Release DataBuffers Manually if Consuming Directly**: If interacting directly with `ServerHttpRequest.getBody()` (Flux of `DataBuffer`), you MUST call `DataBufferUtils.release(dataBuffer)` to return memory to Netty's pool.
4. **Use Reactor Context for Security**: Never rely on `SecurityContextHolder` with `ThreadLocal` strategy in WebFlux. Use `ReactiveSecurityContextHolder`.
5. **Always Supply Request Timeouts**: Bounded `.timeout()` on all remote reactive pipelines prevents orphaned open sockets.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Using Spring Data JPA in WebFlux** | JPA/JDBC is inherently blocking; it stalls Netty event-loop threads and crashes throughput. | Migrate to Spring Data R2DBC or offload to `Schedulers.boundedElastic()`. |
| **Calling `.block()` on Controller Return** | Defeats the entire purpose of WebFlux and risks `IllegalStateException` on event loops. | Return `Mono<T>` or `Flux<T>` directly from controller methods. |
| **Leaking Netty `DataBuffer` Memory** | Not consuming or releasing `DataBuffer` leaks off-heap native memory (`DirectBufferPool`). | Use `DataBufferUtils.release(dataBuffer)` or let message codecs handle it. |
| **Mixing Web and WebFlux Starters** | Including both `spring-boot-starter-web` and `webflux` causes Boot to default to Tomcat MVC! | Include ONLY `spring-boot-starter-webflux` for pure WebFlux. |
| **Using `ThreadLocal` in Interceptors** | Event loops multiplex thousands of requests across a handful of threads, causing state corruption. | Use `WebFilter` with Reactor `contextWrite()`. |
| **Unbounded SSE Stream Memory** | Infinite `Flux.interval()` without client disconnect cancellation can leak server resources. | Monitor client disconnects; WebFlux cancels upstream publisher automatically. |

---

## 8. Performance & Security Considerations

### Off-Heap Direct Memory Overhead
Reactor Netty allocates direct byte buffers outside the JVM heap (`-XX:MaxDirectMemorySize`). If your application processes high volumes of multipart uploads or raw byte streaming, tune direct memory limits to prevent container OOM killer termination.

### Slowloris & Request Body Flooding
Because WebFlux handles thousands of connections on few threads, an attacker streaming bytes extremely slowly can exhaust connection pools. Enforce strict connection timeouts via `server.netty.connection-timeout` and `server.netty.idle-timeout`.

---

## 9. Comparison: Spring MVC vs Spring WebFlux Architecture

| Architectural Layer | Spring MVC (Servlet Stack) | Spring WebFlux (Reactive Stack) |
| :--- | :--- | :--- |
| **Server Engine** | Embedded Tomcat, Jetty (Servlet 6.0) | Reactor Netty (Default), Undertow |
| **Core Front Controller** | `DispatcherServlet` | `DispatcherHandler` |
| **Request / Response API** | `HttpServletRequest` / `Response` | `ServerHttpRequest` / `ServerWebExchange` |
| **Thread Model** | Thread per Request (Platform or Virtual) | Small fixed Event-Loop Pool (1 per CPU core) |
| **Data Persistence** | JPA, Hibernate, JDBC | R2DBC, Reactive Mongo, Reactive Redis |
| **Backpressure** | No (Blocking socket I/O) | Native End-to-End Reactive Streams |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: What is the role of `DispatcherHandler` in Spring WebFlux?</strong></summary>

**Answer:**
`DispatcherHandler` is the reactive equivalent of Spring MVC's `DispatcherServlet`. It implements `WebHandler` and coordinates incoming `ServerWebExchange` processing. It queries `HandlerMapping` beans (discovering target controller methods or router functions), invokes the handler via a `HandlerAdapter`, and delegates the resulting `Mono<HandlerResult>` to a `HandlerResultHandler` to stream serialized bytes to the client socket without blocking.
</details>

<details>
<summary><strong>Q2: Why does Spring Boot default to Spring MVC if both `spring-boot-starter-web` and `spring-boot-starter-webflux` are on the classpath?</strong></summary>

**Answer:**
Spring Boot's auto-configuration evaluates `WebApplicationType`. If Servlet container classes (`Servlet`, `DispatcherServlet`) are present on the classpath, it detects `WebApplicationType.SERVLET` and launches an embedded Tomcat server with Spring MVC. To run WebFlux, you must either remove `spring-boot-starter-web` or explicitly configure `spring.main.web-application-type=reactive`.
</details>

<details>
<summary><strong>Q3: What is `ServerWebExchange` and what does it replace?</strong></summary>

**Answer:**
`ServerWebExchange` is the central HTTP contract in Spring WebFlux. It replaces both `HttpServletRequest` and `HttpServletResponse` from the Servlet specification. It provides non-blocking access to request and response abstractions (`ServerHttpRequest`, `ServerHttpResponse`), session attributes, multipart data, and the subscriber's reactive `Context`.
</details>

<details>
<summary><strong>Q4: How many worker threads does Reactor Netty spawn by default in a WebFlux application?</strong></summary>

**Answer:**
Reactor Netty allocates an event-loop thread pool whose size equals the number of available CPU cores (or a minimum of 4). For example, on an 8-core CPU, Netty typically runs 8 event-loop threads (`reactor-http-nio-*`) to handle all concurrent network I/O.
</details>

<details>
<summary><strong>Q5: What happens if a developer introduces a blocking database call (e.g. Hibernate/JDBC) in a WebFlux controller?</strong></summary>

**Answer:**
The blocking call halts the Netty event-loop thread executing it. Because there are only as many event loops as CPU cores, blocking one thread freezes 12.5% to 25% of the server's entire capacity. If multiple blocking requests arrive concurrently, all event loops freeze, completely halting the application and failing health checks.
</details>

<details>
<summary><strong>Q6: How does WebFlux handle streaming responses differently from Spring MVC?</strong></summary>

**Answer:**
In Spring MVC, streaming requires holding open an asynchronous servlet context and writing to `StreamingResponseBody` or `ResponseBodyEmitter` on a separate thread pool. In WebFlux, streaming is native: returning a `Flux<T>` automatically streams items over HTTP chunked transfer encoding as they are emitted by the publisher, pausing emission when the client's TCP window fills up (backpressure).
</details>

<details>
<summary><strong>Q7: How do you access Spring Security user credentials in WebFlux?</strong></summary>

**Answer:**
In WebFlux, you cannot use `SecurityContextHolder.getContext()` because it relies on `ThreadLocal`. Instead, use `ReactiveSecurityContextHolder.getContext()`, which returns a `Mono<SecurityContext>` by looking up the authenticated `Authentication` principal stored in the Reactor `Context`.
</details>

<details>
<summary><strong>Q8: What is the purpose of `DataBuffer` in WebFlux?</strong></summary>

**Answer:**
`DataBuffer` is Spring's abstraction over byte buffers, unifying Netty's `ByteBuf` and Java's `ByteBuffer`. It allows zero-copy socket transfers and pooling for maximum I/O performance.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Basic Reactive Endpoint</strong></summary>

**Task**: Write a `@RestController` endpoint `GET /api/ping` that returns `Mono<String>` emitting `"PONG"`.

```java
// Solution:
@RestController
public class PingController {
    @GetMapping("/api/ping")
    public Mono<String> ping() {
        return Mono.just("PONG");
    }
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Stream Numbers with Delay</strong></summary>

**Task**: Create an endpoint `GET /api/stream-numbers` producing `TEXT_EVENT_STREAM_VALUE` that emits numbers 1 to 10 with a 200ms delay between numbers.

```java
// Solution:
@GetMapping(value = "/api/stream-numbers", produces = MediaType.TEXT_EVENT_STREAM_VALUE)
public Flux<Integer> streamNumbers() {
    return Flux.range(1, 10).delayElements(Duration.ofMillis(200));
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Reactive WebFilter Header Verification</strong></summary>

**Task**: Implement a `WebFilter` that checks for header `X-API-KEY`. If absent, return HTTP 401 Unauthorized immediately without calling downstream filters.

```java
// Solution:
@Component
public class ApiKeyWebFilter implements WebFilter {
    @Override
    public Mono<Void> filter(ServerWebExchange exchange, WebFilterChain chain) {
        String key = exchange.getRequest().getHeaders().getFirst("X-API-KEY");
        if (key == null || !"valid-key".equals(key)) {
            exchange.getResponse().setStatusCode(HttpStatus.UNAUTHORIZED);
            return exchange.getResponse().setComplete(); // Halts chain
        }
        return chain.filter(exchange);
    }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Custom Reactive Exception Handler</strong></summary>

**Task**: Write an `@ExceptionHandler` inside a `@RestControllerAdvice` that catches `NoSuchElementException` and returns `Mono<ProblemDetail>` with HTTP 404.

```java
// Solution:
@RestControllerAdvice
public class ReactiveExceptionHandler {
    @ExceptionHandler(NoSuchElementException.class)
    public Mono<ProblemDetail> handleNotFound(NoSuchElementException ex) {
        ProblemDetail problem = ProblemDetail.forStatusAndDetail(HttpStatus.NOT_FOUND, ex.getMessage());
        return Mono.just(problem);
    }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Zero-Copy Reactive File Downloader</strong></summary>

**Task**: Build a WebFlux controller endpoint that streams a 500MB file from disk to the client using `ZeroCopyHttpOutputMessage` or `DataBufferUtils.readAsynchronousFileChannel()`, ensuring zero heap allocation.
</details>

<details>
<summary><strong>Exercise 6 (Hard): Custom Reactive WebSocket Handler with Backpressure</strong></summary>

**Task**: Implement a `WebSocketHandler` in WebFlux that receives incoming client messages as a `Flux<WebSocketMessage>`, filters them, and emits backpressured responses to the client session.
</details>

---

## 12. Summary & Quick Reference

- **Core Engine**: `DispatcherHandler` + `ServerWebExchange` (No Servlets).
- **Default Server**: Reactor Netty with event loops (1 thread per core).
- **Golden Rule**: NEVER block on an event-loop thread.
- **Data Layer**: Use Spring Data R2DBC, not JPA/Hibernate.
- **Security**: Use `ReactiveSecurityContextHolder` (Context-bound, not ThreadLocal).

---

## 13. Further Reading

- [Spring Framework Reference: WebFlux Framework](https://docs.spring.io/spring-framework/reference/web/webflux.html)
- [Reactor Netty Documentation](https://projectreactor.io/docs/netty/release/reference/index.html)
- [R2DBC: Reactive Relational Database Connectivity Specification](https://r2dbc.io/)
- [BlockHound: Automatic Blocking Code Detection](https://github.com/reactor/BlockHound)
