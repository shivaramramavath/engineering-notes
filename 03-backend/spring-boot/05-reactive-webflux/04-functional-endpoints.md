# Functional Endpoints & RouterFunctions in Spring WebFlux

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | WebFlux Architecture, Java Lambda Expressions, Functional Interfaces |
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
9. [Comparison: Annotated Controllers vs Functional Endpoints](#9-comparison-annotated-controllers-vs-functional-endpoints)
10. [Interview Questions & Deep Dives](#10-interview-questions--deep-dives)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

In addition to traditional `@RestController` annotated classes, Spring WebFlux introduces an alternative lightweight, functional programming model for defining HTTP routes: **Functional Endpoints** (also known as WebFlux.fn).

### The Problem with Annotated Controllers
- **Reflection & Bytecode Overhead**: `@RestController` relies heavily on runtime annotation scanning, reflection, and dynamic proxy generation.
- **Scattered Routing**: In large applications, routes are scattered across dozens of individual controller files, making it hard to see the complete API surface at a glance.
- **Complex Testing**: Unit testing an annotated controller typically requires spinning up mock MVC/WebFlux environments (`@WebFluxTest`) with reflection-based argument resolvers.

### The Solution: Functional Endpoints (`RouterFunction` & `HandlerFunction`)
Functional Endpoints decouple **routing** from **request handling**:
1. **`HandlerFunction`**: A functional interface `(ServerRequest) -> Mono<ServerResponse>` that processes the request and returns a response.
2. **`RouterFunction`**: A functional interface `(ServerRequest) -> Mono<HandlerFunction>` that evaluates a `RequestPredicate` (method, path, headers) and routes to a handler function.
Because it uses standard Java lambdas and method references, it compiles down to direct, non-reflective code with faster startup and straightforward unit testing.

---

## 2. Core Concepts

### Mental Model & Execution Flow
```
                     Incoming HTTP Request: GET /api/v1/orders/101
                                      │
                                      ▼
                        RouterFunctionMapping
                                      │
                                      ▼
           RouterFunction.route(request) evaluates predicates:
           • Path: /api/v1/orders/{id}               (MATCH)
           • Method: GET                             (MATCH)
           • Accept: application/json                (MATCH)
                                      │
                                      ▼
              Returns Target HandlerFunction (Method Reference):
                    OrderHandler::getOrderById
                                      │
                                      ▼
              Executes: orderHandler.getOrderById(serverRequest)
                                      │
                                      ▼
                         Returns Mono<ServerResponse>
                                      │
                                      ▼
              Writes HTTP Status, Headers, and Body to Netty
```

### Invariants
1. **Immutability**: Both `ServerRequest` and `ServerResponse` instances are immutable. Transformations are performed via builder patterns (`ServerResponse.ok().body(...)`).
2. **Zero Annotation Reflection**: Functional routes do not use reflection to parse route mappings or arguments; parameter extraction is explicit (`request.pathVariable("id")`).
3. **Composable Routing**: Router functions can be composed, nested, and chained using `.andRoute()`, `.nest()`, and `.filter()`.

---

## 3. How It Works Internally

### The `RouterFunction` Contract
```java
@FunctionalInterface
public interface RouterFunction<T extends ServerResponse> {
    Mono<HandlerFunction<T>> route(ServerRequest request);

    default RouterFunction<T> and(RouterFunction<T> other) { ... }
    default RouterFunction<T> filter(HandlerFilterFunction<T, T> filterFunction) { ... }
}
```

1. Spring registers a `RouterFunctionMapping` bean that scans the application context for all beans returning `RouterFunction<?>`.
2. When a request arrives, `RouterFunctionMapping` invokes `routerFunction.route(serverRequest)`.
3. If the `RequestPredicate` matches, it returns a `Mono<HandlerFunction>`.
4. Spring's `HandlerFunctionAdapter` invokes `handlerFunction.handle(serverRequest)`.
5. The resulting `Mono<ServerResponse>` is written directly to the `ServerWebExchange` output stream.

---

## 4. Syntax & Configuration

### Dependency Setup (`pom.xml`)
Functional endpoints are built into `spring-boot-starter-webflux`:
```xml
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-webflux</artifactId>
</dependency>
```

---

## 5. Practical Examples

### Example 1: Separation of Concerns: Dedicated Handler and Router

#### Step 1: The Handler Component (`OrderHandler.java`)
```java
package com.example.webflux.fn.handler;

import org.springframework.http.MediaType;
import org.springframework.stereotype.Component;
import org.springframework.web.reactive.function.server.ServerRequest;
import org.springframework.web.reactive.function.server.ServerResponse;
import reactor.core.publisher.Mono;

import java.net.URI;
import java.util.UUID;

public record OrderDto(UUID id, String item, double amount) {}
public record CreateOrderDto(String item, double amount) {}

@Component
public class OrderHandler {

    public Mono<ServerResponse> getOrderById(ServerRequest request) {
        UUID id = UUID.fromString(request.pathVariable("id"));
        OrderDto order = new OrderDto(id, "Mechanical Keyboard", 149.99);

        return ServerResponse.ok()
                .contentType(MediaType.APPLICATION_JSON)
                .bodyValue(order);
    }

    public Mono<ServerResponse> createOrder(ServerRequest request) {
        return request.bodyToMono(CreateOrderDto.class)
                .map(dto -> new OrderDto(UUID.randomUUID(), dto.item(), dto.amount()))
                .flatMap(created -> ServerResponse
                        .created(URI.create("/api/v1/orders/" + created.id()))
                        .contentType(MediaType.APPLICATION_JSON)
                        .bodyValue(created));
    }
}
```

#### Step 2: The Router Configuration (`OrderRouter.java`)
```java
package com.example.webflux.fn.router;

import com.example.webflux.fn.handler.OrderHandler;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.http.MediaType;
import org.springframework.web.reactive.function.server.RouterFunction;
import org.springframework.web.reactive.function.server.RouterFunctions;
import org.springframework.web.reactive.function.server.ServerResponse;

import static org.springframework.web.reactive.function.server.RequestPredicates.*;

@Configuration(proxyBeanMethods = false)
public class OrderRouter {

    @Bean
    public RouterFunction<ServerResponse> orderRoutes(OrderHandler handler) {
        return RouterFunctions.route()
                .path("/api/v1/orders", builder -> builder
                        .nest(accept(MediaType.APPLICATION_JSON), nestedBuilder -> nestedBuilder
                                .GET("/{id}", handler::getOrderById)
                                .POST(handler::createOrder)
                        )
                )
                .build();
    }
}
```

### Example 2: Nested Routing with Composable Path Predicates

```java
package com.example.webflux.fn.router;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.web.reactive.function.server.RouterFunction;
import org.springframework.web.reactive.function.server.ServerResponse;

import static org.springframework.web.reactive.function.server.RequestPredicates.path;
import static org.springframework.web.reactive.function.server.RouterFunctions.nest;
import static org.springframework.web.reactive.function.server.RouterFunctions.route;

@Configuration(proxyBeanMethods = false)
public class NestedModularApiRouter {

    @Bean
    public RouterFunction<ServerResponse> apiRoutes(UserRoutes userRoutes, ProductRoutes productRoutes) {
        // Compose separate functional modules under a unified /api/v2 prefix
        return nest(path("/api/v2"),
                userRoutes.routes().and(productRoutes.routes())
        );
    }
}

interface UserRoutes { RouterFunction<ServerResponse> routes(); }
interface ProductRoutes { RouterFunction<ServerResponse> routes(); }
```

### Example 3: Edge Case — Applying a `HandlerFilterFunction` (Functional Middleware)

`HandlerFilterFunction` allows intercepting requests before reaching the handler function (e.g. for timing, security, or audit logging).

```java
package com.example.webflux.fn.filter;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.web.reactive.function.server.HandlerFilterFunction;
import org.springframework.web.reactive.function.server.RouterFunction;
import org.springframework.web.reactive.function.server.RouterFunctions;
import org.springframework.web.reactive.function.server.ServerResponse;

@Configuration(proxyBeanMethods = false)
public class FilteredRouterConfig {

    private static final Logger log = LoggerFactory.getLogger(FilteredRouterConfig.class);

    @Bean
    public RouterFunction<ServerResponse> auditedRoutes() {
        return RouterFunctions.route()
                .GET("/api/v1/health", request -> ServerResponse.ok().bodyValue("UP"))
                // Apply functional filter
                .filter(executionTimingFilter())
                .build();
    }

    private HandlerFilterFunction<ServerResponse, ServerResponse> executionTimingFilter() {
        return (request, next) -> {
            long start = System.currentTimeMillis();
            return next.handle(request).doOnSuccess(response -> {
                long duration = System.currentTimeMillis() - start;
                log.info("Request [{}] {} took {}ms (Status: {})",
                        request.method(), request.path(), duration, response.statusCode());
            });
        };
    }
}
```

### Example 4: Anti-Pattern vs Production Fix

#### Anti-Pattern: Giant Monolithic Router Class with Embedded Business Logic
```java
// ANTI-PATTERN: Embedding complex business logic directly into router lambdas
@Bean
public RouterFunction<ServerResponse> badRouter() {
    return route(GET("/users/{id}"), req -> {
        // Bloated lambda: DB logic, error handling, validation all inlined!
        String id = req.pathVariable("id");
        return database.findById(id)
            .flatMap(u -> ServerResponse.ok().bodyValue(u));
    });
}
```

#### Production Fix: Separate Handler Methods from Route Declarations
```java
// PRODUCTION FIX: Clean routing definitions pointing to injected Handler component
@Bean
public RouterFunction<ServerResponse> goodRouter(UserHandler handler) {
    return route()
        .GET("/users/{id}", handler::getUserById)
        .build();
}
```

### Example 5: High-Performance Pure Unit Testing of Functional Handlers

Because `HandlerFunction` takes `ServerRequest` and returns `Mono<ServerResponse>`, handlers can be unit-tested directly using `MockServerRequest` without spinning up any HTTP server!

```java
package com.example.webflux.fn.test;

import com.example.webflux.fn.handler.OrderHandler;
import org.junit.jupiter.api.Test;
import org.springframework.http.HttpStatus;
import org.springframework.mock.web.reactive.function.server.MockServerRequest;
import org.springframework.web.reactive.function.server.ServerResponse;
import reactor.core.publisher.Mono;
import reactor.test.StepVerifier;

import java.util.UUID;

class OrderHandlerTest {

    private final OrderHandler orderHandler = new OrderHandler();

    @Test
    void testGetOrderById() {
        UUID testId = UUID.randomUUID();
        // Zero-overhead mock request: No Netty, no MockMvc, instantaneous execution!
        MockServerRequest request = MockServerRequest.builder()
                .pathVariable("id", testId.toString())
                .build();

        Mono<ServerResponse> responseMono = orderHandler.getOrderById(request);

        StepVerifier.create(responseMono)
                .assertNext(response -> {
                    org.assertj.core.api.Assertions.assertThat(response.statusCode()).isEqualTo(HttpStatus.OK);
                })
                .verifyComplete();
    }
}
```

---

## 6. Best Practices

1. **Always Separate Handlers from Routers**: Keep `RouterFunction` definitions strictly declarative (defining paths and predicates) and delegate execution to `@Component` handlers.
2. **Use the Router Builder Pattern**: Use `RouterFunctions.route().path(...).build()` for clean, nested, self-documenting routing definitions.
3. **Leverage `HandlerFilterFunction` for Middleware**: Implement cross-cutting concerns (authentication, timing, logging) using functional filters instead of global filters when only specific routes need them.
4. **Unit Test Handlers with `MockServerRequest`**: Write fast unit tests for handler functions by constructing `MockServerRequest` directly without launching WebFlux server slices.
5. **Enforce Media Type Predicates**: Always include `.and(accept(MediaType.APPLICATION_JSON))` to reject non-matching media requests early.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Inlining Heavy Logic in Routers** | Creates unreadable, untestable spaghetti code in configuration classes. | Extract logic into a dedicated `@Component` handler class. |
| **Missing Path Variables** | Calling `request.pathVariable("id")` on a route that didn't declare `{id}` throws `IllegalArgumentException`. | Verify path template declares matching variable `{id}`. |
| **Consuming Request Body Multiple Times** | In WebFlux, the request body stream cannot be read twice. | Read into a `Mono<T>` once and chain downstream operators. |
| **Returning Raw Objects from Handlers** | Handlers must strictly return `Mono<ServerResponse>`, NOT raw POJOs or `Mono<User>`. | Wrap payloads in `ServerResponse.ok().bodyValue(...)`. |
| **Forgetting `.build()` on Route Builder** | The router bean remains unconstructed or misconfigured. | Always terminate `RouterFunctions.route()...` with `.build()`. |
| **Route Ordering Ambiguities** | Defining `GET("/**")` before specific routes shadows the specific endpoints. | Declare specific routes before catch-all wildcard routes. |

---

## 8. Performance & Security Considerations

### Startup Time & GraalVM Native Image Compilation
Because Functional Endpoints eliminate reflection-based method scanning (`@RequestMapping`), they compile faster and more reliably with **GraalVM Native Image** AOT (Ahead-Of-Time) compilation. For serverless applications (AWS Lambda, Google Cloud Run) requiring instant sub-10ms cold starts, Functional Endpoints are significantly faster than annotated controllers.

### Input Validation
Unlike `@Valid @RequestBody UserDto` in annotated controllers, Functional Endpoints do not trigger automatic JSR-380 validation out of the box. You must inject a Spring `Validator` into your handler and validate the extracted body explicitly.

---

## 9. Comparison: Annotated Controllers vs Functional Endpoints

| Feature | Annotated Controllers (`@RestController`) | Functional Endpoints (WebFlux.fn) |
| :--- | :--- | :--- |
| **Programming Paradigm** | Declarative Annotations | Functional Lambdas & Method References |
| **Route Visibility** | Scattered across multiple classes | Centralized in clean `RouterFunction` beans |
| **Reflection Overhead** | Moderate (Reflection-heavy) | Zero (Pure Java method references) |
| **GraalVM AOT / Native** | Supported (Requires reflection hints) | Best-in-Class (Minimal reflection) |
| **Automatic Validation** | Built-in (`@Valid`) | Requires manual `Validator` invocation |
| **Unit Testing Speed** | Requires `@WebFluxTest` / MockMvc | Instantaneous via `MockServerRequest` |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: What are the two core interfaces that make up Spring WebFlux Functional Endpoints?</strong></summary>

**Answer:**
1. **`HandlerFunction<T extends ServerResponse>`**: Represents the function that handles a request: `Mono<T> handle(ServerRequest request)`. It accepts an immutable `ServerRequest` and returns a `Mono<ServerResponse>`.
2. **`RouterFunction<T extends ServerResponse>`**: Represents the routing function: `Mono<HandlerFunction<T>> route(ServerRequest request)`. It evaluates predicates (path, HTTP method, headers) against the request and returns a matching `HandlerFunction`.
</details>

<details>
<summary><strong>Q2: Why do Functional Endpoints start up faster than `@RestController` annotated controllers?</strong></summary>

**Answer:**
Annotated controllers require Spring to scan the entire classpath, inspect class and method reflection metadata for `@RequestMapping`, parse parameter types, and configure CGLIB proxy adapters. Functional Endpoints are declared as direct Java method references and lambda chains. The JVM executes them as standard bytecode without runtime reflection scanning, reducing startup time and memory footprint.
</details>

<details>
<summary><strong>Q3: How do you handle input validation with Jakarta Bean Validation (Hibernate Validator) in a functional handler?</strong></summary>

**Answer:**
Inject `org.springframework.validation.Validator` into the handler class. After extracting the body with `request.bodyToMono(MyDto.class)`, invoke `validator.validate(dto, errors)`. If `errors.hasErrors()`, return `ServerResponse.badRequest().bodyValue(errors.getAllErrors())` or throw a custom exception.
</details>

<details>
<summary><strong>Q4: What is `HandlerFilterFunction` in WebFlux.fn?</strong></summary>

**Answer:**
`HandlerFilterFunction<T, R>` is functional middleware that wraps a `HandlerFunction`. It intercepts requests before they reach the handler and can modify the incoming `ServerRequest` or the outgoing `ServerResponse`, enabling cross-cutting concerns like route-specific authentication, execution metrics, and header manipulation.
</details>

<details>
<summary><strong>Q5: Can you mix `@RestController` classes and `RouterFunction` beans in the same Spring Boot application?</strong></summary>

**Answer:**
Yes. Spring WebFlux includes both `RequestMappingHandlerMapping` and `RouterFunctionMapping` in its context. A project can use annotated controllers for standard CRUD endpoints while using functional endpoints for streaming, health checks, or specialized routing.
</details>

<details>
<summary><strong>Q6: How does path nesting work in `RouterFunctions`?</strong></summary>

**Answer:**
Using `RouterFunctions.nest(RequestPredicate, RouterFunction)` or the builder `.path("/base", b -> b.GET(...))`. The nested routes inherit the parent path prefix and any shared predicates (e.g. content-type or header requirements).
</details>

<details>
<summary><strong>Q7: How do you extract query parameters in a `HandlerFunction`?</strong></summary>

**Answer:**
Using `request.queryParam("paramName")`, which returns an `Optional<String>`. For multiple values, use `request.queryParams()`, which returns a `MultiValueMap<String, String>`.
</details>

<details>
<summary><strong>Q8: What is `MockServerRequest`?</strong></summary>

**Answer:**
`MockServerRequest` is a test utility provided by `spring-test` (`org.springframework.mock.web.reactive.function.server.MockServerRequest`). It allows developers to construct mock request instances with custom headers, paths, and bodies, enabling true unit testing of handler functions without spinning up a servlet container or Netty server.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Basic Functional Route</strong></summary>

**Task**: Write a `RouterFunction<ServerResponse>` that routes `GET /ping` to return `"PONG"` with HTTP 200.

```java
// Solution:
@Bean
public RouterFunction<ServerResponse> pingRoute() {
    return RouterFunctions.route(
            RequestPredicates.GET("/ping"),
            request -> ServerResponse.ok().bodyValue("PONG")
    );
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Extract Path Variable in Handler</strong></summary>

**Task**: Write a handler lambda that reads path variable `name` from `GET /hello/{name}` and returns `"Hello, " + name`.

```java
// Solution:
@Bean
public RouterFunction<ServerResponse> helloRoute() {
    return RouterFunctions.route(
            RequestPredicates.GET("/hello/{name}"),
            req -> ServerResponse.ok().bodyValue("Hello, " + req.pathVariable("name"))
    );
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Nested Route with JSON Predicate</strong></summary>

**Task**: Build a `RouterFunction` using the builder pattern that nests routes under `/api/customers`, supporting `GET` (returns list) and `POST` (consumes JSON).

```java
// Solution:
@Bean
public RouterFunction<ServerResponse> customerRoutes(CustomerHandler handler) {
    return RouterFunctions.route()
            .path("/api/customers", builder -> builder
                    .GET(handler::listCustomers)
                    .POST(RequestPredicates.accept(MediaType.APPLICATION_JSON), handler::createCustomer)
            )
            .build();
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Custom Authentication FilterFunction</strong></summary>

**Task**: Implement a `HandlerFilterFunction` that checks for header `X-Auth-Token = secret123`; if missing, returns HTTP 401 Unauthorized.

```java
// Solution:
HandlerFilterFunction<ServerResponse, ServerResponse> authFilter() {
    return (request, next) -> {
        String token = request.headers().firstHeader("X-Auth-Token");
        if (!"secret123".equals(token)) {
            return ServerResponse.status(HttpStatus.UNAUTHORIZED).bodyValue("Unauthorized");
        }
        return next.handle(request);
    };
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Explicit Programmatic JSR-380 Validation Handler</strong></summary>

**Task**: Implement a generic utility method in a handler that accepts `ServerRequest`, extracts a Java record, runs Jakarta Bean Validation via `org.springframework.validation.Validator`, and returns HTTP 400 with a map of field errors if invalid.
</details>

<details>
<summary><strong>Exercise 6 (Hard): Server-Sent Events Functional Router</strong></summary>

**Task**: Implement a functional handler that returns `ServerResponse.ok().contentType(MediaType.TEXT_EVENT_STREAM).body(Flux.interval(...))` streaming continuous server events.
</details>

---

## 12. Summary & Quick Reference

- **Core Paradigm**: Functional routing via `RouterFunction` + `HandlerFunction`.
- **Zero Reflection**: Fast startup and GraalVM native image friendly.
- **Builder API**: `RouterFunctions.route().path(...).build()`.
- **Unit Testing**: Instantaneous testing via `MockServerRequest`.
- **Middleware**: Attach `HandlerFilterFunction` via `.filter(...)`.

---

## 13. Further Reading

- [Spring Framework Reference: WebFlux.fn Functional Endpoints](https://docs.spring.io/spring-framework/reference/web/webflux-functional.html)
- [Project Reactor Official Documentation](https://projectreactor.io/docs)
- [Spring Boot with GraalVM Native Image Support](https://docs.spring.io/spring-boot/reference/packaging/native-image/index.html)
- [Functional Reactive Microservices Architecture](https://spring.io/guides)
