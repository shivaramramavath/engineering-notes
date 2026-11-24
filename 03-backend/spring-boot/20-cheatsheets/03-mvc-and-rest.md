# Spring Web MVC & REST APIs Cheatsheet

| Metadata | Details |
|---|---|
| **Topic** | Spring Web MVC & REST API Syntax Cheatsheet |
| **Track** | Java Spring Boot |
| **Difficulty** | Intermediate |
| **Prerequisites** | Java 21, HTTP Protocol, REST Architecture |
| **Java Version** | Java 21 / 25 LTS |
| **Spring Boot** | 3.4.x / 4.x |
| **Tested With** | Spring Boot Starter Web, MockMvc |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [Internals & Architecture](#3-internals--architecture)
4. [Syntax & Categorized Reference](#4-syntax--categorized-reference)
5. [5 Practical Production Examples](#5-5-practical-production-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes to Avoid](#7-common-mistakes-to-avoid)
8. [Performance & Security Considerations](#8-performance--security-considerations)
9. [Comparison Table: Web MVC vs WebFlux](#9-comparison-table-web-mvc-vs-webflux)
10. [Technical Interview Questions & Answers](#10-technical-interview-questions--answers)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading & References](#13-further-reading--references)

---

## 1. What & Why
Spring Web MVC is the foundational framework for building HTTP-based web applications and RESTful APIs in the Spring ecosystem. With Spring Boot 3+ on Java 21, Spring MVC natively integrates RFC 9457 / RFC 7807 `ProblemDetail` error responses, JSR-380 validation, content negotiation, and virtual-thread-friendly blocking I/O models.

This cheatsheet provides immediate lookups for mapping syntax, parameter extraction, response headers, status codes, and exception interception.

---

## 2. Core Concepts
- **DispatcherServlet**: Front controller dispatching incoming HTTP requests to mapped handler methods.
- **HandlerMapping & HandlerAdapter**: Resolves controller endpoints and invokes method signatures with parameter resolution.
- **HttpMessageConverter**: Serializes and deserializes payloads (e.g. Jackson `MappingJackson2HttpMessageConverter`).
- **Validation**: Declarative payload assertions (`@Valid`, `@Validated`) with Jakarta Bean Validation.
- **ProblemDetail (RFC 9457)**: Standardized JSON payload structure for API errors introduced natively in Spring 6 / Boot 3.

---

## 3. Internals & Architecture
HTTP Request Lifecycle through Spring DispatcherServlet:

```
[HTTP Client Request]
        |
        v
+-------------------------------------------------------------+
|                     DispatcherServlet                       |
+-------------------------------------------------------------+
        |
        | 1. Query HandlerMapping
        v
+-------------------------------------------------------------+
|             RequestMappingHandlerMapping                    |
|    - Matches URL pattern, HTTP method, headers, produces    |
+-------------------------------------------------------------+
        |
        | 2. Obtain HandlerExecutionChain (Interceptors)
        v
+-------------------------------------------------------------+
|             RequestMappingHandlerAdapter                    |
|    - HandlerMethodArgumentResolverComposite                 |
|      (@PathVariable, @RequestParam, @RequestBody)          |
|    - HttpMessageConverter (Jackson JSON deserialization)    |
|    - DataBinder & Jakarta Validator                         |
+-------------------------------------------------------------+
        |
        | 3. Invoke Controller Method
        v
+-------------------------------------------------------------+
|                   Target Controller Method                  |
+-------------------------------------------------------------+
        |
        | 4. HandlerMethodReturnValueHandlerComposite
        v
+-------------------------------------------------------------+
|         MappingJackson2HttpMessageConverter (Serialize)     |
+-------------------------------------------------------------+
        |
        v
[HTTP Client Response: 200 OK / 201 Created / 400 Bad Request]
```

---

## 4. Syntax & Categorized Reference

### 4.1 Endpoint Routing & Request Binding
| Annotation | Example Syntax | Purpose |
|---|---|---|
| `@GetMapping` | `@GetMapping("/orders/{id}")` | Maps HTTP GET |
| `@PostMapping` | `@PostMapping(consumes = MediaType.APPLICATION_JSON_VALUE)` | Maps HTTP POST |
| `@PutMapping` | `@PutMapping("/{id}")` | Idempotent resource replacement |
| `@PatchMapping` | `@PatchMapping("/{id}")` | Partial resource update |
| `@DeleteMapping` | `@DeleteMapping("/{id}")` | Resource removal |
| `@PathVariable` | `@PathVariable("id") UUID orderId` | URI path segment extraction |
| `@RequestParam` | `@RequestParam(name = "page", defaultValue = "0") int page` | Query string extraction |
| `@RequestHeader` | `@RequestHeader("X-Correlation-Id") String traceId` | HTTP header extraction |
| `@RequestBody` | `@RequestBody @Valid OrderCreateRequest body` | Request body JSON deserialization |

### 4.2 Standard HTTP Response Headers & Status Codes
```java
// Standard return formats
return ResponseEntity.status(HttpStatus.CREATED)
        .header(HttpHeaders.LOCATION, "/api/v1/orders/" + id)
        .body(response);

// 204 No Content
return ResponseEntity.noContent().build();

// 200 OK with ETag
return ResponseEntity.ok()
        .eTag(version)
        .body(data);
```

---

## 5. 5 Practical Production Examples

### Example 1: Standard REST Controller with Pagination & Filtering
```java
@RestController
@RequestMapping("/api/v1/products")
public class ProductController {

    private final ProductService productService;

    public ProductController(ProductService productService) {
        this.productService = productService;
    }

    @GetMapping
    public ResponseEntity<PageResponse<ProductDto>> listProducts(
            @RequestParam(required = false) String category,
            @RequestParam(defaultValue = "0") int page,
            @RequestParam(defaultValue = "20") int size) {
        return ResponseEntity.ok(productService.search(category, PageRequest.of(page, size)));
    }
}
```

### Example 2: REST Controller with Creation & URI Location
```java
@PostMapping
public ResponseEntity<ProductDto> createProduct(
        @Valid @RequestBody ProductCreateRequest request,
        UriComponentsBuilder ucb) {
    ProductDto created = productService.create(request);
    URI location = ucb.path("/api/v1/products/{id}").buildAndExpand(created.id()).toUri();
    return ResponseEntity.created(location).body(created);
}
```

### Example 3: Global RFC 9457 ProblemDetail Handler
```java
@RestControllerAdvice
public class GlobalExceptionHandler extends ResponseEntityExceptionHandler {

    @ExceptionHandler(ResourceNotFoundException.class)
    public ProblemDetail handleNotFound(ResourceNotFoundException ex) {
        ProblemDetail problem = ProblemDetail.forStatusAndDetail(HttpStatus.NOT_FOUND, ex.getMessage());
        problem.setTitle("Resource Not Found");
        problem.setType(URI.create("https://api.example.com/errors/not-found"));
        problem.setProperty("timestamp", Instant.now());
        return problem;
    }

    @ExceptionHandler(BusinessRuleViolationException.class)
    public ProblemDetail handleBusinessViolation(BusinessRuleViolationException ex) {
        ProblemDetail problem = ProblemDetail.forStatusAndDetail(HttpStatus.CONFLICT, ex.getMessage());
        problem.setTitle("Business Rule Violation");
        problem.setProperty("errorCode", ex.getErrorCode());
        return problem;
    }
}
```

### Example 4: Custom DTO Validation Constraints
```java
public record UserRegistrationRequest(
    @NotBlank(message = "Email is required")
    @Email(message = "Invalid email format")
    String email,

    @NotBlank(message = "Password is required")
    @Size(min = 8, max = 64, message = "Password must be 8-64 characters")
    String password,

    @Min(value = 18, message = "Age must be at least 18")
    int age
) {}
```

### Example 5: Streaming Response via ResponseBodyEmitter
```java
@GetMapping(value = "/stream/events", produces = MediaType.TEXT_EVENT_STREAM_VALUE)
public ResponseBodyEmitter streamEvents() {
    ResponseBodyEmitter emitter = new ResponseBodyEmitter(60_000L);
    CompletableFuture.runAsync(() -> {
        try {
            for (int i = 0; i < 5; i++) {
                emitter.send("Event " + i + " at " + Instant.now() + "\n");
                Thread.sleep(1000);
            }
            emitter.complete();
        } catch (Exception ex) {
            emitter.completeWithError(ex);
        }
    });
    return emitter;
}
```

---

## 6. Best Practices
1. **Always Return `ResponseEntity` or Structured DTOs**: Explicit HTTP statuses prevent unintended default 200 OK on empty/null responses.
2. **Standardize on RFC 9457 `ProblemDetail`**: Adopt consistent error schema across all microservices.
3. **Validate All Inbound Requests**: Apply `@Valid` on `@RequestBody` parameters and validate records.
4. **Enforce Content Negotiation**: Explicitly declare `consumes` and `produces` headers on sensitive endpoints.
5. **Use `UriComponentsBuilder` for Location Headers**: Generates RFC-compliant absolute URIs for HTTP 201 Created responses.

---

## 7. Common Mistakes to Avoid

| Mistake | Consequence | Remedy |
|---|---|---|
| Returning entity models directly from controllers | Causes infinite recursion on bi-directional relationships and leaks database internals | Always map entities to immutable DTO records |
| Catching generic `Exception` inside controller methods | Hides errors and bypasses global `@RestControllerAdvice` | Let exceptions propagate to `@RestControllerAdvice` |
| Omitting `@Valid` on `@RequestBody` | Jakarta Bean Validation annotations inside DTO records are ignored | Add `@Valid` to the controller method argument |
| Returning 200 OK on resource creation | Violates REST protocol expectations | Return HTTP 201 Created with `Location` header |
| Inadequate validation of `@PathVariable` IDs | Throws unhandled type conversion errors | Use UUID or typed domain primitives with global conversion handler |
| Manual JSON parsing via `ObjectMapper` in controller | Redundant boilerplate; bypasses Spring pipeline | Rely on Spring MVC's automated `@RequestBody` deserialization |

---

## 8. Performance & Security Considerations
- **Enable Virtual Threads**: Set `spring.threads.virtual.enabled=true` to handle 10,000+ concurrent blocking requests per instance.
- **Prevent JSON Injection**: Ensure Jackson has Polymorphic Typing controls enabled and default typing disabled.
- **CORS Policies**: Explicitly define `@CrossOrigin` or centralized `CorsConfigurationSource` to prevent cross-origin data theft.

---

## 9. Comparison Table: Web MVC vs WebFlux

| Metric | Spring Web MVC | Spring WebFlux |
|---|---|---|
| **Programming Model** | Imperative / Blocking (Virtual Thread ready) | Reactive / Non-blocking (Reactor Mono/Flux) |
| **Concurrency Engine** | Tomcat / Jetty (Platform or Virtual threads) | Netty EventLoop |
| **Backpressure** | OS TCP Socket buffer | Reactive Streams Backpressure |
| **JDBC / JPA Support** | Native, full ecosystem | R2DBC only (no JPA / Hibernate ORM) |

---

## 10. Technical Interview Questions & Answers

<details>
<summary>1. What is the role of DispatcherServlet in Spring MVC?</summary>
`DispatcherServlet` acts as the Front Controller, orchestrating the request lifecycle by delegating to `HandlerMapping`, invoking adapters, resolving arguments, serializing responses, and routing exceptions.
</details>

<details>
<summary>2. How does RFC 9457 ProblemDetail work in Spring Boot 3?</summary>
`ProblemDetail` is a native Spring 6 representation of standardized error details containing status, title, detail, type URI, and custom extension properties, automatically serialized to JSON.
</details>

<details>
<summary>3. What is the difference between `@PathVariable` and `@RequestParam`?</summary>
`@PathVariable` extracts values directly embedded in the URI path (`/users/{id}`), whereas `@RequestParam` extracts query string key-value pairs (`/users?status=active`).
</details>

<details>
<summary>4. Why should controllers never return JPA Entities directly?</summary>
Direct entity return exposes persistence state, invites serialization loops (lazy associations), and can trigger accidental lazy-loading queries outside of transactions.
</details>

<details>
<summary>5. How does Content Negotiation function in Spring MVC?</summary>
Spring evaluates the `Accept` header against the controller's `produces` attributes and configured `HttpMessageConverter` beans to serialize the response into JSON, XML, or binary.
</details>

<details>
<summary>6. What is the difference between `@ControllerAdvice` and `@RestControllerAdvice`?</summary>
`@RestControllerAdvice` is a composed annotation that combines `@ControllerAdvice` and `@ResponseBody`, ensuring all error responses are automatically serialized to JSON.
</details>

<details>
<summary>7. How does Spring MVC validate nested collections in request bodies?</summary>
By placing `@Valid` on the collection field inside the parent DTO: `List<@Valid ItemDto> items`.
</details>

<details>
<summary>8. How do virtual threads affect Spring MVC controller performance?</summary>
Virtual threads allow Spring MVC to retain familiar synchronous, blocking code while scaling to hundreds of thousands of concurrent I/O operations without reactive complexity.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary>Exercise 1: Build a REST CRUD controller for Order management</summary>
Implement endpoints for GET, POST, PUT, DELETE using Java records and `ResponseEntity`.
</details>

<details>
<summary>Exercise 2: Implement RFC 9457 error handling</summary>
Create a global `@RestControllerAdvice` handling `MethodArgumentNotValidException` with detailed field error mappings.
</details>

<details>
<summary>Exercise 3: Test controller validation with MockMvc</summary>
Write a test using MockMvc verifying that submitting an invalid email returns HTTP 400 Bad Request.
</details>

<details>
<summary>Exercise 4: Implement pagination with custom PageResponse record</summary>
Write a controller endpoint returning a paginated response with total pages, page index, and content.
</details>

<details>
<summary>Exercise 5: Add ETag-based optimistic concurrency</summary>
Implement an endpoint that compares HTTP `If-Match` headers against entity versions.
</details>

<details>
<summary>Exercise 6: Implement Server-Sent Events (SSE)</summary>
Create an endpoint returning `SseEmitter` that emits stock price updates periodically.
</details>

---

## 12. Summary & Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                    SPRING WEB MVC / REST CHEAT SHEET                            |
+---------------------------------------------------------------------------------+
| ROUTING       | @GetMapping, @PostMapping, @PutMapping, @DeleteMapping          |
| PARAMS        | @PathVariable, @RequestParam, @RequestHeader, @RequestBody      |
| VALIDATION    | @Valid, @NotNull, @NotBlank, @Size, @Email                      |
| RESPONSES     | ResponseEntity.ok(body), ResponseEntity.created(uri).body(dto)  |
| ERRORS        | ProblemDetail.forStatusAndDetail(HttpStatus, detail)            |
| GLOBAL ADVICE | @RestControllerAdvice, extends ResponseEntityExceptionHandler   |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading & References
- [Spring MVC Official Documentation](https://docs.spring.io/spring-framework/reference/web/webmvc.html)
- [RFC 9457: Problem Details for HTTP APIs](https://www.rfc-editor.org/rfc/rfc9457.html)
- [Jakarta Bean Validation 3.0 Specification](https://jakarta.ee/specifications/bean-validation/3.0/)
- [Spring REST Docs and Testing Guide](https://docs.spring.io/spring-boot/docs/current/reference/html/features.html#features.testing.spring-boot-applications.spring-mvc-tests)
