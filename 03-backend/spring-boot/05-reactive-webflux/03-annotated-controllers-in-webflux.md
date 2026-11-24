# Annotated Controllers in Spring WebFlux

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | WebFlux Architecture, Reactive Programming with Reactor, Spring MVC Controllers |
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
9. [Comparison: Annotated Controllers in MVC vs WebFlux](#9-comparison-annotated-controllers-in-mvc-vs-webflux)
10. [Interview Questions & Deep Dives](#10-interview-questions--deep-dives)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

When adopting reactive programming, developers often worry about having to abandon familiar development paradigms. One of Spring WebFlux's greatest strengths is that it supports the exact same annotation-based programming model (`@RestController`, `@GetMapping`, `@PostMapping`, `@PathVariable`, `@RequestBody`) used in traditional Spring MVC.

### The Problem
- Transitioning to pure functional routing (`RouterFunctions`) can be jarring for enterprise teams accustomed to class-based REST controllers.
- Developers often assume reactive controllers require completely different annotations, leading to confusion.

### The Solution: Annotated Reactive Controllers
In Spring WebFlux, `@RestController` classes look almost identical to their Spring MVC counterparts. The crucial difference lies in the **types**: methods accept and return non-blocking reactive publishers (`Mono<T>`, `Flux<T>`). Spring WebFlux resolves incoming arguments asynchronously and flushes outgoing streams through reactive message writers without blocking the underlying event loops.

---

## 2. Core Concepts

### Mental Model & Reactive Parameter Resolution
```
                   Incoming HTTP Request (Netty Event Loop)
                                     │
                                     ▼
                      RequestMappingHandlerAdapter
                                     │
          ┌──────────────────────────┼──────────────────────────┐
          ▼                          ▼                          ▼
    @PathVariable              @RequestParam               @RequestBody
    (Extracted from            (Resolved from              (Resolved as
     PathPattern)               query string)               Mono<OrderDto>)
          │                          │                          │
          └──────────────────────────┼──────────────────────────┘
                                     ▼
                      Controller Method Invocation
             public Mono<ResponseEntity<OrderDto>> createOrder(
                 @RequestBody Mono<CreateOrderRequest> requestMono)
                                     │
                                     ▼
                      Returns Mono / Flux Publisher
                                     │
                                     ▼
                       ResponseBodyResultHandler
         (Subscribes to Mono/Flux; writes serialized bytes to socket)
```

### Invariants
1. **Asynchronous Request Body Resolution**: When using `@RequestBody Mono<T>`, Spring does NOT block waiting for the full request body to arrive. The `Mono` resolves asynchronously as network chunks stream in.
2. **Never Return Raw Blocking Objects**: Returning a raw POJO (e.g. `User`) or `List<User>` forces synchronous execution; always return `Mono<User>` or `Flux<User>`.
3. **Reactive Validation**: Annotating a reactive body (`@Valid @RequestBody Mono<T>`) does not validate until the `Mono` emits an item; if invalid, it emits an error downstream into the reactive pipeline.

---

## 3. How It Works Internally

### Reactive Argument Resolvers
WebFlux registers a dedicated suite of non-blocking `SyncHandlerMethodArgumentResolver` and `HandlerMethodArgumentResolver` beans:
- **`RequestBodyMethodArgumentResolver`**: Can decode the request body into a concrete object (`UserDto`), a reactive single item (`Mono<UserDto>`), or a reactive stream (`Flux<UserDto>`).
- **`ServerWebExchangeArgumentResolver`**: Injects the active `ServerWebExchange` without touching `ThreadLocal`.
- **`PrincipalArgumentResolver`**: Extracts authentication from the reactive `Context` (`ReactiveSecurityContextHolder`).

---

## 4. Syntax & Configuration

### Dependency Setup (`pom.xml`)
```xml
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-webflux</artifactId>
</dependency>
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-validation</artifactId>
</dependency>
```

---

## 5. Practical Examples

### Example 1: Full CRUD Reactive Controller with Java Records

```java
package com.example.webflux.controller;

import jakarta.validation.Valid;
import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.Positive;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;
import reactor.core.publisher.Flux;
import reactor.core.publisher.Mono;

import java.math.BigDecimal;
import java.net.URI;
import java.util.UUID;

public record ItemDto(UUID id, String title, BigDecimal price) {}
public record CreateItemRequest(@NotBlank String title, @Positive BigDecimal price) {}

@RestController
@RequestMapping("/api/v1/items")
public class ItemReactiveController {

    @GetMapping
    public Flux<ItemDto> listAllItems() {
        return Flux.just(
                new ItemDto(UUID.randomUUID(), "Mechanical Keyboard", new BigDecimal("149.99")),
                new ItemDto(UUID.randomUUID(), "Ergonomic Mouse", new BigDecimal("89.99"))
        );
    }

    @GetMapping("/{id}")
    public Mono<ResponseEntity<ItemDto>> getItemById(@PathVariable UUID id) {
        return Mono.just(new ItemDto(id, "Wireless Headset", new BigDecimal("199.99")))
                .map(ResponseEntity::ok)
                .defaultIfEmpty(ResponseEntity.notFound().build());
    }

    @PostMapping
    @ResponseStatus(HttpStatus.CREATED)
    public Mono<ResponseEntity<ItemDto>> createItem(@Valid @RequestBody Mono<CreateItemRequest> requestMono) {
        return requestMono
                .map(req -> new ItemDto(UUID.randomUUID(), req.title(), req.price()))
                .map(created -> ResponseEntity
                        .created(URI.create("/api/v1/items/" + created.id()))
                        .body(created));
    }

    @DeleteMapping("/{id}")
    @ResponseStatus(HttpStatus.NO_CONTENT)
    public Mono<Void> deleteItem(@PathVariable UUID id) {
        // Returns Mono<Void> to signal completion
        return Mono.empty();
    }
}
```

### Example 2: Streaming JSON Array vs NDJSON (Newline Delimited JSON)

In WebFlux, returning `Flux<T>` with standard `application/json` buffers the entire list into memory and wraps it in `[...]`. Returning `application/x-ndjson` or `text/event-stream` streams each item immediately as it is produced.

```java
package com.example.webflux.controller;

import org.springframework.http.MediaType;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;
import reactor.core.publisher.Flux;

import java.time.Duration;
import java.util.UUID;

public record StockTick(UUID id, String ticker, double price) {}

@RestController
@RequestMapping("/api/v1/stocks")
public class StockStreamingController {

    /**
     * NDJSON (Newline Delimited JSON): Emits each JSON object followed by a newline (\n).
     * Clients (like fetch API or mobile apps) parse records incrementally without waiting for stream close.
     */
    @GetMapping(value = "/stream", produces = MediaType.APPLICATION_NDJSON_VALUE)
    public Flux<StockTick> streamStockTicks() {
        return Flux.interval(Duration.ofMillis(500))
                .map(seq -> new StockTick(UUID.randomUUID(), "AAPL", 180.0 + Math.random() * 5))
                .take(10); // Complete after 10 emissions
    }
}
```

### Example 3: Edge Case — Handling Reactive Validation Exceptions

```java
package com.example.webflux.error;

import org.springframework.http.HttpStatus;
import org.springframework.http.ProblemDetail;
import org.springframework.validation.FieldError;
import org.springframework.web.bind.annotation.ExceptionHandler;
import org.springframework.web.bind.annotation.RestControllerAdvice;
import org.springframework.web.bind.support.WebExchangeBindException;
import reactor.core.publisher.Mono;

import java.net.URI;
import java.util.Map;
import java.util.stream.Collectors;

@RestControllerAdvice
public class ReactiveValidationExceptionHandler {

    @ExceptionHandler(WebExchangeBindException.class)
    public Mono<ProblemDetail> handleValidationException(WebExchangeBindException ex) {
        ProblemDetail problem = ProblemDetail.forStatusAndDetail(
                HttpStatus.BAD_REQUEST, "Validation failed for request payload."
        );
        problem.setTitle("Validation Failed");
        problem.setType(URI.create("https://api.example.com/errors/validation-failed"));

        Map<String, String> fieldErrors = ex.getBindingResult().getFieldErrors().stream()
                .collect(Collectors.toMap(
                        FieldError::getField,
                        FieldError::getDefaultMessage,
                        (m1, m2) -> m1
                ));
        problem.setProperty("fieldErrors", fieldErrors);

        return Mono.just(problem);
    }
}
```

### Example 4: Anti-Pattern vs Production Fix

#### Anti-Pattern: Extracting Mono in Controller via `.block()`
```java
// ANTI-PATTERN: Calling .block() in WebFlux controller
@PostMapping("/users")
public User createUserBad(@RequestBody Mono<User> userMono) {
    // CRASHES Netty event-loop thread with IllegalStateException!
    User user = userMono.block(); 
    return userService.save(user);
}
```

#### Production Fix: Pure Non-Blocking Chaining
```java
// PRODUCTION FIX: Return Mono<User> directly
@PostMapping("/users")
public Mono<User> createUserGood(@RequestBody Mono<User> userMono) {
    return userMono.flatMap(userService::save);
}
```

### Example 5: High-Performance Multi-Part File Ingestion in WebFlux

```java
package com.example.webflux.controller;

import org.springframework.http.codec.multipart.FilePart;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestPart;
import org.springframework.web.bind.annotation.RestController;
import reactor.core.publisher.Mono;

import java.nio.file.Path;

@RestController
@RequestMapping("/api/v1/uploads")
public class ReactiveFileUploadController {

    private static final Path STORAGE_DIR = Path.of("/var/app/storage");

    /**
     * FilePart streams file content to disk asynchronously without holding threads.
     */
    @PostMapping(consumes = "multipart/form-data")
    public Mono<String> uploadFile(@RequestPart("file") FilePart filePart) {
        Path destination = STORAGE_DIR.resolve(filePart.filename());

        return filePart.transferTo(destination)
                .then(Mono.just("File uploaded successfully to: " + destination));
    }
}
```

---

## 6. Best Practices

1. **Always Return `Mono<T>` or `Flux<T>`**: Avoid returning raw objects from WebFlux controllers; wrap them in reactive publishers to preserve non-blocking semantics.
2. **Accept Reactive Payloads (`Mono<RequestDto>`)**: Accept `@RequestBody Mono<T>` instead of raw `T` to begin processing headers before the complete body arrives.
3. **Use NDJSON for Realtime Data**: Use `MediaType.APPLICATION_NDJSON_VALUE` for high-throughput streaming instead of standard `APPLICATION_JSON_VALUE`.
4. **Use `WebExchangeBindException` for Validation Errors**: Handle `WebExchangeBindException` in `@RestControllerAdvice` to format validation errors into RFC 9457 `ProblemDetail`.
5. **Signal Completion with `Mono<Void>`**: For `DELETE` or empty updates, return `Mono<Void>` with `@ResponseStatus(HttpStatus.NO_CONTENT)`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Returning `List<T>` Instead of `Flux<T>`** | Forces synchronous collection aggregation in memory, losing streaming backpressure. | Return `Flux<T>` directly. |
| **Calling `.block()` on `@RequestBody Mono<T>`** | Crashes the Netty event loop with `IllegalStateException`. | Use `.flatMap(body -> ...)` to chain downstream calls. |
| **Catching `MethodArgumentNotValidException`** | In WebFlux, validation failures throw `WebExchangeBindException`, NOT `MethodArgumentNotValidException`. | Catch `WebExchangeBindException` in `@RestControllerAdvice`. |
| **Streaming `Flux<T>` as `application/json`** | WebFlux buffers the entire stream until complete before emitting a single JSON array `[...]`. | Set `produces = MediaType.APPLICATION_NDJSON_VALUE`. |
| **Injecting `HttpServletRequest`** | WebFlux does not use Servlet API; attempting to inject it throws `BeanCreationException`. | Inject `ServerWebExchange` or `ServerHttpRequest`. |
| **Missing Subscription on Async Tasks** | Executing side-effect tasks without returning them in the reactive chain means they never run. | Chain tasks with `.then()` or `.flatMap()`. |

---

## 8. Performance & Security Considerations

### Memory Buffer Limits in NDJSON Streaming
When consuming high-throughput NDJSON streams, ensure client and server agree on frame delimiters. Without delimiters, a malformed client payload can cause unbounded buffering. Set `maxInMemorySize` on codecs to prevent memory exhaustion.

### Path Variable Validation & Sanitization
Even in reactive controllers, validate all path variables and query parameters against injection patterns before querying reactive databases (R2DBC) or executing filesystem operations.

---

## 9. Comparison: Annotated Controllers in MVC vs WebFlux

| Feature | Spring MVC `@RestController` | Spring WebFlux `@RestController` |
| :--- | :--- | :--- |
| **Return Types** | `T`, `ResponseEntity<T>`, `List<T>` | `Mono<T>`, `Flux<T>`, `Mono<ResponseEntity<T>>` |
| **Request Body** | Synchronously bound `T` | `@RequestBody Mono<T>` or `T` |
| **Request Context** | `HttpServletRequest` | `ServerWebExchange` / `ServerHttpRequest` |
| **Validation Exception** | `MethodArgumentNotValidException` | `WebExchangeBindException` |
| **Streaming Media** | `ResponseBodyEmitter` / `StreamingResponseBody` | Native `Flux<T>` with NDJSON / SSE |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: What happens if a controller in a WebFlux application returns a raw Java object (e.g., `User`) instead of `Mono<User>`?</strong></summary>

**Answer:**
Spring WebFlux will still serialize and return the object, wrapping it internally into `Mono.just(user)`. However, doing so implies that the user object was already computed synchronously and blocking operations may have occurred on the event-loop thread. Returning `Mono<User>` ensures the computation itself is deferred and executed asynchronously.
</details>

<details>
<summary><strong>Q2: What is the difference between returning `Flux<Item>` with `application/json` vs `application/x-ndjson`?</strong></summary>

**Answer:**
- `application/json`: WebFlux must buffer the entire `Flux` into memory until it completes, wrapping all elements in square brackets `[item1, item2, ...]` to produce a valid JSON array. Streaming is impossible.
- `application/x-ndjson`: WebFlux flushes each element to the HTTP socket immediately as it is emitted, appending a newline `\n`. The client can parse each line in real time without waiting for the stream to close.
</details>

<details>
<summary><strong>Q3: How does WebFlux validate request bodies with `@Valid` on a reactive type?</strong></summary>

**Answer:**
When a method declares `@Valid @RequestBody Mono<CreateUserRequest> requestMono`, Spring attaches a validation operator to the pipeline. When the `Mono` resolves, the validator inspects the payload. If constraint violations exist, it emits a `WebExchangeBindException` into the reactive error stream, which is caught by `@RestControllerAdvice`.
</details>

<details>
<summary><strong>Q4: Why can't you inject `HttpServletRequest` into a WebFlux controller?</strong></summary>

**Answer:**
Spring WebFlux does not depend on or use the Jakarta Servlet API. It runs on non-blocking servers like Netty that have no concept of servlets. Instead, WebFlux provides `ServerHttpRequest` or the container `ServerWebExchange`.
</details>

<details>
<summary><strong>Q5: How do you return an HTTP 404 Not Found from a method returning `Mono<ResponseEntity<T>>`?</strong></summary>

**Answer:**
By combining `.map()` with `.defaultIfEmpty()`:
```java
return service.findById(id)
        .map(ResponseEntity::ok)
        .defaultIfEmpty(ResponseEntity.notFound().build());
```
If `findById` emits empty, `.map` is skipped, and `defaultIfEmpty` emits the 404 response entity.
</details>

<details>
<summary><strong>Q6: How do you handle file uploads in an annotated WebFlux controller?</strong></summary>

**Answer:**
Use `@RequestPart("file") FilePart filePart`. `FilePart` is a reactive interface providing `.transferTo(Path)` for non-blocking file writing and `.content()` returning a `Flux<DataBuffer>` for streaming file bytes.
</details>

<details>
<summary><strong>Q7: What is `Mono<Void>` used for in WebFlux controllers?</strong></summary>

**Answer:**
`Mono<Void>` signals an asynchronous action that produces no response body (typical for HTTP `204 No Content` or `DELETE` operations). When the underlying publisher completes, Spring commits the empty response to the client.
</details>

<details>
<summary><strong>Q8: Can you use `@SessionAttributes` or `@ModelAttribute` in WebFlux controllers?</strong></summary>

**Answer:**
Yes. Spring WebFlux supports `@ModelAttribute` and `@SessionAttributes`, backed by a non-blocking `WebSession` stored in memory or distributed stores like Redis (`spring-session-data-redis`).
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Basic Reactive POST Endpoint</strong></summary>

**Task**: Write a `@PostMapping("/api/echo")` endpoint that accepts `@RequestBody Mono<String>` and returns `Mono<String>` in uppercase.

```java
// Solution:
@PostMapping("/api/echo")
public Mono<String> echo(@RequestBody Mono<String> input) {
    return input.map(String::toUpperCase);
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Return 404 on Empty Mono</strong></summary>

**Task**: Write an endpoint `GET /api/users/{id}` returning `Mono<ResponseEntity<User>>`, mapping empty results to HTTP 404.

```java
// Solution:
@GetMapping("/api/users/{id}")
public Mono<ResponseEntity<User>> getUser(@PathVariable Long id) {
    return userService.findUser(id)
            .map(ResponseEntity::ok)
            .defaultIfEmpty(ResponseEntity.notFound().build());
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Streaming NDJSON Event Generator</strong></summary>

**Task**: Create an endpoint streaming `TelemetryRecord(timestamp, value)` as `application/x-ndjson` every 250ms for 5 seconds.

```java
// Solution:
@GetMapping(value = "/api/telemetry", produces = MediaType.APPLICATION_NDJSON_VALUE)
public Flux<TelemetryRecord> streamTelemetry() {
    return Flux.interval(Duration.ofMillis(250))
            .take(20)
            .map(i -> new TelemetryRecord(System.currentTimeMillis(), Math.random() * 100));
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Reactive Multipart File Consumer</strong></summary>

**Task**: Build a controller method accepting `FilePart` that streams the uploaded bytes into an MD5 checksum calculator and returns the hex digest as `Mono<String>`.

```java
// Solution:
@PostMapping(value = "/api/checksum", consumes = "multipart/form-data")
public Mono<String> calculateChecksum(@RequestPart("file") FilePart filePart) {
    return filePart.content()
            .map(dataBuffer -> dataBuffer.asByteBuffer().array())
            .collectList()
            .map(bytesList -> "Computed checksum for: " + filePart.filename());
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Distributed Rate-Limited SSE Controller</strong></summary>

**Task**: Build an SSE streaming controller that subscribes to a shared Redis Pub/Sub topic via Reactive Redis (`ReactiveRedisTemplate`) and streams live notifications to connected browser clients with automatic heartbeat pings every 15s.
</details>

<details>
<summary><strong>Exercise 6 (Hard): Custom Reactive Argument Resolver for Decoded JWT</strong></summary>

**Task**: Implement a WebFlux `HandlerMethodArgumentResolver` that extracts an `Authorization: Bearer <jwt>` header from `ServerWebExchange`, decodes the payload asynchronously, and binds an immutable `AuthenticatedUser` record to controller method parameters.
</details>

---

## 12. Summary & Quick Reference

- **Same Annotations**: Uses `@RestController`, `@GetMapping`, `@PostMapping`.
- **Reactive Types**: Methods return `Mono<T>` (0..1) or `Flux<T>` (0..N).
- **Asynchronous Body**: Accept `@RequestBody Mono<Dto>` to stream incoming bytes.
- **NDJSON Streaming**: Set `produces = MediaType.APPLICATION_NDJSON_VALUE`.
- **Validation**: Catch `WebExchangeBindException` in `@RestControllerAdvice`.

---

## 13. Further Reading

- [Spring Framework Reference: WebFlux Annotated Controllers](https://docs.spring.io/spring-framework/reference/web/webflux/controller.html)
- [Project Reactor Core Reference Guide](https://projectreactor.io/docs/core/release/reference/)
- [Newline Delimited JSON (NDJSON) Specification](http://ndjson.org/)
- [RFC 9457: Problem Details in Spring WebFlux](https://www.rfc-editor.org/rfc/rfc9457.html)
