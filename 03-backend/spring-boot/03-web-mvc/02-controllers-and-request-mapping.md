# Controllers & Request Mapping in Spring MVC

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate to Advanced |
| **Prerequisites** | DispatcherServlet Lifecycle, Spring IoC, Java Annotations |
| **Estimated Reading Time** | 22 minutes |
| **Status** | Production-Ready |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | OpenJDK 21 LTS / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 21.0.2, Spring Boot 3.4.0 |

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
9. [Comparison: `@Controller` vs `@RestController`](#9-comparison-controller-vs-restcontroller)
10. [Interview Questions & Deep Dives](#10-interview-questions--deep-dives)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

In modern web development, routing HTTP requests to appropriate handlers must be clean, declarative, type-safe, and capable of expressing complex matching criteria (HTTP verbs, URI patterns, query parameters, header constraints, and media types).

### The Problem
Without an expressive routing mechanism:
1. Endpoints require manual `if-else` branching on request paths and headers.
2. Content negotiation (JSON vs XML) requires custom header parsing inside every handler.
3. Path parameter extraction and type casting require repetitive string manipulation.

### The Solution: Spring MVC Mapping Abstractions
Spring MVC uses declarative annotations (`@RestController`, `@RequestMapping`, `@GetMapping`, `@PostMapping`, etc.) powered by `RequestMappingHandlerMapping`. This maps incoming HTTP requests to specific controller methods based on fine-grained path patterns, header conditions, and MIME types.

---

## 2. Core Concepts

### Mental Model: Request Mapping Resolution Matrix
```
                       Incoming HTTP Request:
                 POST /api/v1/orders/ORD-992 HTTP/1.1
                 Content-Type: application/json
                 Accept: application/json
                 Header: X-Api-Version=2
                               │
                               ▼
            RequestMappingHandlerMapping Match Matrix:
  ┌────────────────────────────────────────────────────────────┐
  │ 1. Path Match:     /api/v1/orders/{orderId}        (MATCH) │
  │ 2. HTTP Method:    RequestMethod.POST              (MATCH) │
  │ 3. Params:         status=active (not specified)   (PASS)  │
  │ 4. Headers:        X-Api-Version=2                 (MATCH) │
  │ 5. Consumes:       application/json                (MATCH) │
  │ 6. Produces:       application/json                (MATCH) │
  └────────────────────────────────────────────────────────────┘
                               │
                               ▼
                  Target HandlerMethod Selected:
        OrderController.submitOrder(String orderId, OrderDto)
```

### Invariants
1. **Specificity Wins**: If multiple mappings match a request, Spring picks the **most specific pattern** (e.g., `/orders/active` matches before `/orders/{id}`).
2. **PathPattern Matching**: Modern Spring Boot uses `PathPatternParser`, which parses paths into an AST at startup for ultra-fast, non-regex matching.
3. **Consumes / Produces Gates**: A request with an incompatible `Content-Type` generates HTTP 415 (`Unsupported Media Type`), and an incompatible `Accept` header generates HTTP 406 (`Not Acceptable`).

---

## 3. How It Works Internally

### Route Registration and Matching
During application context initialization:
1. `RequestMappingHandlerMapping` implements `InitializingBean`. Its `afterPropertiesSet()` scans all beans for classes annotated with `@Controller` or `@RequestMapping`.
2. For every annotated method, it constructs a `RequestMappingInfo` object containing path patterns, HTTP methods, params, headers, consumes, and produces conditions.
3. It registers each route in an internal `MappingRegistry` containing URL lookup tables.

```java
// Conceptual mapping registry lookup
public HandlerMethod getHandlerInternal(HttpServletRequest request) {
    String lookupPath = initLookupPath(request);
    List<Match> matches = new ArrayList<>();
    
    // Step 1: Direct path lookup for exact matches (O(1) hash map)
    List<T> directPathMatches = this.mappingRegistry.getMappingsByDirectPath(lookupPath);
    if (directPathMatches != null) {
        addMatchingMappings(directPathMatches, matches, request);
    }
    
    // Step 2: Pattern-based matching (wildcards, path variables)
    if (matches.isEmpty()) {
        addMatchingMappings(this.mappingRegistry.getRegistrations().keySet(), matches, request);
    }
    
    // Step 3: Sort matches using Comparator (most specific wins)
    if (!matches.isEmpty()) {
        Comparator<Match> comparator = new MatchComparator(getMappingComparator(request));
        matches.sort(comparator);
        return matches.get(0).getHandlerMethod();
    }
    return null;
}
```

---

## 4. Syntax & Configuration

### Composed Shortcut Annotations
Spring provides convenient aliases for `@RequestMapping(method = ...)`:
- `@GetMapping` $\equiv$ `@RequestMapping(method = RequestMethod.GET)`
- `@PostMapping` $\equiv$ `@RequestMapping(method = RequestMethod.POST)`
- `@PutMapping` $\equiv$ `@RequestMapping(method = RequestMethod.PUT)`
- `@DeleteMapping` $\equiv$ `@RequestMapping(method = RequestMethod.DELETE)`
- `@PatchMapping` $\equiv$ `@RequestMapping(method = RequestMethod.PATCH)`

### Comprehensive Mapping Attributes

```java
@RequestMapping(
    value = "/orders",
    method = RequestMethod.POST,
    params = "stage=checkout",             // Matches only if ?stage=checkout is present
    headers = "X-Client-Type=Mobile",       // Matches only if header matches
    consumes = MediaType.APPLICATION_JSON_VALUE, // Matches Content-Type
    produces = MediaType.APPLICATION_JSON_VALUE  // Matches Accept header
)
```

---

## 5. Practical Examples

### Example 1: Clean RESTful CRUD Controller with Records

```java
package com.example.web.controller;

import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.net.URI;
import java.util.List;
import java.util.UUID;

public record ProductDto(UUID id, String name, double price) {}
public record CreateProductRequest(String name, double price) {}

@RestController
@RequestMapping("/api/v1/products")
public class ProductController {

    @GetMapping
    public List<ProductDto> listProducts(@RequestParam(defaultValue = "0") int page,
                                         @RequestParam(defaultValue = "20") int size) {
        return List.of(new ProductDto(UUID.randomUUID(), "Mechanical Keyboard", 149.99));
    }

    @GetMapping("/{id}")
    public ResponseEntity<ProductDto> getProduct(@PathVariable UUID id) {
        return ResponseEntity.ok(new ProductDto(id, "Wireless Mouse", 79.99));
    }

    @PostMapping
    @ResponseStatus(HttpStatus.CREATED)
    public ResponseEntity<ProductDto> createProduct(@RequestBody CreateProductRequest request) {
        UUID newId = UUID.randomUUID();
        ProductDto created = new ProductDto(newId, request.name(), request.price());
        return ResponseEntity.created(URI.create("/api/v1/products/" + newId)).body(created);
    }

    @DeleteMapping("/{id}")
    public ResponseEntity<Void> deleteProduct(@PathVariable UUID id) {
        return ResponseEntity.noContent().build();
    }
}
```

### Example 2: Enterprise Controller with Header, Param, and Media Constraints

```java
package com.example.web.controller;

import org.springframework.http.MediaType;
import org.springframework.web.bind.annotation.*;

@RestController
@RequestMapping("/api/v1/reports")
public class ReportController {

    // Matches: GET /api/v1/reports?format=pdf Accept: application/pdf
    @GetMapping(
        params = "format=pdf",
        produces = MediaType.APPLICATION_PDF_VALUE
    )
    public byte[] downloadPdfReport() {
        return new byte[]{0x25, 0x50, 0x44, 0x46}; // PDF magic bytes
    }

    // Matches: GET /api/v1/reports?format=csv Accept: text/csv
    @GetMapping(
        params = "format=csv",
        produces = "text/csv"
    )
    public String downloadCsvReport() {
        return "id,name,amount\n1,Widget,500\n";
    }

    // Matches only if header X-Partner-Key is present and not empty
    @PostMapping(
        headers = "X-Partner-Key",
        consumes = MediaType.APPLICATION_JSON_VALUE
    )
    public String submitPartnerBatch(@RequestBody String payload) {
        return "Batch accepted from verified partner";
    }
}
```

### Example 3: Edge Case — Path Variable Ambiguity and Precedence

```java
package com.example.web.controller;

import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

@RestController
@RequestMapping("/api/v1/users")
public class UserRoutingPrecedenceController {

    // Literal path: HIGHEST PRECEDENCE
    @GetMapping("/me")
    public String getCurrentUserProfile() {
        return "Returning profile of authenticated caller (literal /me)";
    }

    // Single segment wildcard path: LOWER PRECEDENCE than literal
    @GetMapping("/{userId}")
    public String getUserById(@PathVariable String userId) {
        return "Returning profile for dynamic user ID: " + userId;
    }

    // Multi-segment path pattern
    @GetMapping("/{userId}/settings/**")
    public String getUserCatchAllSettings(@PathVariable String userId) {
        return "Returning nested settings for user ID: " + userId;
    }
}
```

### Example 4: Anti-Pattern vs Production Fix

#### Anti-Pattern: Using Generic `@RequestMapping` with Raw Status Codes
```java
// ANTI-PATTERN: Ambiguous method verbs, missing types, returning untyped maps
@Controller
public class BadOrderController {

    // Missing explicit HTTP method (defaults to ALL: GET, POST, PUT, DELETE!)
    @RequestMapping("/order/update")
    @ResponseBody
    public Object updateOrder(HttpServletRequest request) {
        // Parsing parameters by hand from request
        String id = request.getParameter("id");
        return Collections.singletonMap("result", "ok");
    }
}
```

#### Production Fix: Specific Mapping with Typed Domain Payloads
```java
// PRODUCTION FIX: Explicit HTTP verb, typed paths, and standard HTTP semantics
@RestController
@RequestMapping("/api/v1/orders")
public class GoodOrderController {

    @PutMapping("/{id}")
    public ResponseEntity<OrderDto> updateOrder(@PathVariable UUID id,
                                                @Valid @RequestBody UpdateOrderRequest request) {
        OrderDto updated = orderService.update(id, request);
        return ResponseEntity.ok(updated);
    }
}
```

### Example 5: High-Performance Keyset Filtering Controller

```java
package com.example.web.controller;

import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;

import java.time.Instant;
import java.util.List;
import java.util.UUID;

public record AuditEvent(UUID id, String eventType, Instant createdAt) {}
public record KeysetPage<T>(List<T> items, String nextCursor, boolean hasMore) {}

@RestController
@RequestMapping("/api/v1/audit-events")
public class AuditEventController {

    @GetMapping
    public KeysetPage<AuditEvent> getAuditEvents(
            @RequestParam(required = false) String cursor,
            @RequestParam(defaultValue = "50") int limit) {
        
        // Fast keyset evaluation avoiding expensive SQL OFFSET
        List<AuditEvent> results = List.of(
            new AuditEvent(UUID.randomUUID(), "LOGIN_SUCCESS", Instant.now())
        );
        return new KeysetPage<>(results, "eyJpZCI6MTAxfQ==", false);
    }
}
```

---

## 6. Best Practices

1. **Always Use Method-Specific Mappings**: Favor `@GetMapping`, `@PostMapping`, etc., over generic `@RequestMapping`. A bare `@RequestMapping` matches all HTTP verbs, creating security vulnerabilities.
2. **Standardize URI Paths**: Use kebab-case for URLs (`/api/v1/order-items`, not `/orderItems` or `/order_items`).
3. **Keep Controllers Thin**: Controllers should only handle HTTP protocol concerns (headers, status codes, DTO serialization). Delegate all business rules to `@Service` classes.
4. **Leverage Consumes & Produces**: Explicitly declare `consumes` and `produces` to enforce strict OpenAPI contracts and prevent ambiguous response types.
5. **Use Immutable Records for DTOs**: In Java 21+, define request and response payloads using Java `record` types to ensure immutability and thread safety.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Using `@Controller` without `@ResponseBody`** | Spring attempts to resolve the return string as a Thymeleaf/JSP view template name, throwing `404` or `TemplateResolutionException`. | Use `@RestController` (which includes `@ResponseBody`). |
| **Ambiguous Method Mapping** | Two methods define identical paths, verbs, and conditions, causing startup failure with `IllegalStateException: Ambiguous mapping`. | Add distinctive params, headers, or distinct sub-paths. |
| **Trailing Slash Inconsistencies** | In Spring Boot 3+, `/api/users` and `/api/users/` are treated as distinct paths by default. | Use consistent trailing-slash policies or configure `UrlPathHelper`. |
| **Missing `@PathVariable` Name Match** | If compiled without `-parameters` flag, `@PathVariable String id` fails unless annotated as `@PathVariable("id")`. | Ensure Maven `maven-compiler-plugin` includes `-parameters` compiler argument. |
| **Catch-All Wildcards Masking Concrete Routes** | Declaring `@GetMapping("/**")` on a controller intercepts all requests, masking specific endpoints. | Avoid broad catch-all wildcards on root paths. |
| **Mutating Shared State in Controller** | Controllers are singletons; storing user session data in instance fields causes concurrent user state corruption. | Keep controllers strictly stateless. |

---

## 8. Performance & Security Considerations

### PathPatternParser vs AntPathMatcher
Spring Boot 3 uses `PathPatternParser` by default. It parses route patterns into an optimized tree structure once during startup, eliminating runtime regular expression evaluations and providing lower per-request dispatch latency.

### Open Redirection & Path Traversal
Never concatenate raw `@PathVariable` inputs directly into file paths or HTTP redirect URLs (`"redirect:" + url`). Always validate and sanitize identifiers against allowed characters or whitelist patterns.

---

## 9. Comparison: `@Controller` vs `@RestController`

| Feature | `@Controller` | `@RestController` |
| :--- | :--- | :--- |
| **Target Architecture** | Server-Rendered HTML (Thymeleaf, JSP) | Stateless RESTful APIs (JSON, XML) |
| **Return Type Handling** | Treated as View Name by `ViewResolver` | Serialized directly to response body via `HttpMessageConverter` |
| **Annotations Included** | `@Component` | `@Controller` + `@ResponseBody` |
| **Primary Method Return** | `String`, `ModelAndView` | `ResponseEntity<T>`, Java `record`, `List<T>` |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: What happens if two `@RequestMapping` methods share identical path and HTTP method conditions?</strong></summary>

**Answer:**
During context refresh, `RequestMappingHandlerMapping` checks for route collisions. If two methods produce identical matching criteria without differentiating parameters, headers, or media types, Spring throws an `IllegalStateException: Ambiguous mapping found`. The application fails to start.
</details>

<details>
<summary><strong>Q2: How does Spring MVC decide which handler method to invoke when multiple mappings match?</strong></summary>

**Answer:**
Spring compares matches using a specialized `Comparator` (`RequestMappingInfo.compareTo`). It prioritizes:
1. Literal paths over wildcard paths (e.g., `/users/new` beats `/users/{id}`).
2. Patterns with fewer wildcards and variables.
3. Specific `params` and `headers` criteria.
4. More specific `consumes` and `produces` media types.
</details>

<details>
<summary><strong>Q3: Why was trailing slash matching disabled by default in Spring Boot 3?</strong></summary>

**Answer:**
Historically, Spring matched `/users` and `/users/` identically. This was disabled to align with modern web RFCs, improve security against caching/proxy bypass attacks (like CVEs involving reverse proxy path normalization discrepancies), and ensure deterministic routing.
</details>

<details>
<summary><strong>Q4: What is the compiler requirement for parameter names in Spring Boot 3?</strong></summary>

**Answer:**
Spring Framework 6+ removed internal bytecode parameter discovery mechanisms (LocalVariableTableParameterNameDiscoverer). To bind `@PathVariable` or `@RequestParam` without specifying explicit names (e.g. `@PathVariable String id` instead of `@PathVariable("id") String id`), the application must be compiled with the `-parameters` flag enabled in `javac`.
</details>

<details>
<summary><strong>Q5: Can `@RequestMapping` be applied at both class and method levels? How do they combine?</strong></summary>

**Answer:**
Yes. The class-level mapping defines the common base path, headers, or media types for all methods in the class. The method-level mapping appends sub-paths and narrows conditions (e.g., specifying HTTP verbs or specific query parameters).
</details>

<details>
<summary><strong>Q6: What is the difference between `consumes` and `produces`?</strong></summary>

**Answer:**
- `consumes`: Checks the incoming `Content-Type` header of the request. Rejects non-matching types with HTTP 415 (Unsupported Media Type).
- `produces`: Checks the incoming `Accept` header of the request and sets the outgoing response `Content-Type`. Rejects non-matching types with HTTP 406 (Not Acceptable).
</details>

<details>
<summary><strong>Q7: How do Matrix Variables work in Spring MVC?</strong></summary>

**Answer:**
Matrix variables allow passing key-value pairs within URL path segments separated by semicolons (e.g., `/cars;color=red;year=2024`). They are disabled by default and require enabling `removeSemicolonContent(false)` on `UrlPathHelper`.
</details>

<details>
<summary><strong>Q8: Is a `@RestController` thread-safe?</strong></summary>

**Answer:**
Yes, provided it is designed correctly. By default, `@RestController` beans are singletons shared across all concurrent worker threads. As long as the controller does not maintain mutable instance variables, it is completely thread-safe.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Create a Versioned Greeting Controller</strong></summary>

**Task**: Write a `@RestController` mapped to `/api/v1/greet` that returns `"Hello, " + name` where `name` is an optional query parameter defaulting to `"World"`.

```java
// Solution:
@RestController
@RequestMapping("/api/v1/greet")
public class GreetingController {
    @GetMapping
    public String greet(@RequestParam(defaultValue = "World") String name) {
        return "Hello, " + name;
    }
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Restrict Endpoint to JSON Consumption</strong></summary>

**Task**: Annotate a POST endpoint `/api/v1/orders` so it accepts only `application/json` and returns HTTP 201 Created.

```java
// Solution:
@PostMapping(
    value = "/api/v1/orders",
    consumes = MediaType.APPLICATION_JSON_VALUE
)
@ResponseStatus(HttpStatus.CREATED)
public OrderResponse submitOrder(@RequestBody OrderRequest request) {
    return new OrderResponse("ORD-1", "ACCEPTED");
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Controller with Custom Header Matching</strong></summary>

**Task**: Create an endpoint `/api/admin/system` that only matches if the header `X-Admin-Role=SUPERUSER` is present.

```java
// Solution:
@RestController
public class SuperAdminController {
    @GetMapping(
        value = "/api/admin/system",
        headers = "X-Admin-Role=SUPERUSER"
    )
    public Map<String, String> getSystemStatus() {
        return Map.of("system", "OPERATIONAL", "securityTier", "SUPERUSER");
    }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Multiple Path Variables with Regex Constraints</strong></summary>

**Task**: Map a GET endpoint `/api/v1/users/{userId:[0-9]+}/roles/{role:[A-Z]+}` to restrict the `userId` to digits and `role` to uppercase letters.

```java
// Solution:
@GetMapping("/api/v1/users/{userId:[0-9]+}/roles/{role:[A-Z]+}")
public String getUserRole(@PathVariable Long userId, @PathVariable String role) {
    return "User " + userId + " has valid role " + role;
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Dynamic Content Negotiation Handler</strong></summary>

**Task**: Build a controller that serves the same analytics report as JSON when `Accept: application/json` is sent, or as CSV when `Accept: text/csv` is sent, using Spring's content negotiation infrastructure.
</details>

<details>
<summary><strong>Exercise 6 (Hard): Custom Routing Condition Implementation</strong></summary>

**Task**: Implement a custom `RequestCondition` that checks for a custom cryptographic HMAC signature in the headers before matching a route.
</details>

---

## 12. Summary & Quick Reference

- **Use `@RestController`**: Combines `@Controller` and `@ResponseBody`.
- **Prefer Composed Annotations**: Use `@GetMapping`, `@PostMapping`, `@PutMapping`, `@DeleteMapping`.
- **Precedence**: Literal paths match before variable paths.
- **Strict Media Matching**: Use `consumes` and `produces` to enforce HTTP contracts.

---

## 13. Further Reading

- [Spring Framework Reference: Annotated Controllers](https://docs.spring.io/spring-framework/reference/web/webmvc/mvc-controller.html)
- [Spring MVC URI Patterns and Path Matching](https://docs.spring.io/spring-framework/reference/web/webmvc/mvc-controller/ann-requestmapping.html#mvc-ann-requestmapping-uri-templates)
- [RFC 9110: HTTP Semantics and Content Negotiation](https://www.rfc-editor.org/rfc/rfc9110.html)
- [Modern Java Web Development with Spring Boot](https://spring.io/guides)
