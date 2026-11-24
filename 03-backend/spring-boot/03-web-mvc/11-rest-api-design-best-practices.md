# REST API Design Best Practices in Spring Boot

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | Spring MVC, HTTP Specifications (RFC 9110), REST Architectural Constraints |
| **Estimated Reading Time** | 25 minutes |
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
9. [Comparison: REST vs RPC vs GraphQL](#9-comparison-rest-vs-rpc-vs-graphql)
10. [Interview Questions & Deep Dives](#10-interview-questions--deep-dives)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Representational State Transfer (REST) is an architectural style defined by Roy Fielding that underpins the modern web. When building distributed microservices, APIs serve as public, immutable contracts between independent organizations, frontend teams, and partner platforms.

### The Problem
Ad-hoc or poor REST API designs lead to:
1. **RPC in Disguise**: Endpoints modeled as actions (`/api/createCustomer`, `/api/deleteUserById`) rather than resources and HTTP verbs.
2. **Inconsistent Status Codes**: Returning `200 OK` for business failures with custom `{ "status": "error" }` payloads.
3. **Idempotency Violations**: Unsafe retry behavior where re-issuing a failed payment request charges the customer twice.
4. **Breaking Changes**: APIs that fail to support graceful backward-compatible evolution.

### The Solution: Idiomatic Spring Boot REST APIs
Spring Boot provides the tools to build production-grade, Richardson Maturity Level 2 and Level 3 RESTful APIs. By adhering strictly to HTTP/1.1 and HTTP/2 semantics, resource naming conventions, idempotent mutations, standardized status codes, and RFC 9457 error contracts, developers can build intuitive, durable, and cache-friendly systems.

---

## 2. Core Concepts

### The Richardson Maturity Model
```
  Level 3: Hypermedia Controls (HATEOAS)
           Discoverable state transitions via hypermedia links (_links)
           ▲
           │
  Level 2: HTTP Verbs & Status Codes
           Proper use of GET, POST, PUT, PATCH, DELETE + standard 2xx, 4xx, 5xx
           ▲
           │
  Level 1: Resources
           Individual URIs for individual entities (/orders/123 vs /orders/456)
           ▲
           │
  Level 0: The Swamp of POX (Plain Old XML/JSON)
           Single endpoint (e.g. /api/service), single verb (POST), tunnel commands
```

### HTTP Method Semantics & Invariants

| HTTP Method | Safe? | Idempotent? | Cacheable? | Primary Semantics |
| :--- | :--- | :--- | :--- | :--- |
| **GET** | Yes | Yes | Yes | Retrieve resource representation |
| **HEAD** | Yes | Yes | Yes | Retrieve headers only (no body) |
| **OPTIONS** | Yes | Yes | No | Discover supported communication options |
| **POST** | No | No | Only if explicit | Create subordinate resource or execute action |
| **PUT** | No | Yes | No | Completely replace existing resource (or create if client provides ID) |
| **PATCH** | No | No (spec) / Yes (in practice) | No | Partially mutate existing resource fields |
| **DELETE** | No | Yes | No | Remove resource |

---

## 3. How It Works Internally

### Resource Modeling Principles
1. **Nouns, Not Verbs**: Resources represent domain entities or collections of entities. Always use plural nouns:
   - Good: `GET /api/v1/orders`, `POST /api/v1/orders`
   - Bad: `GET /api/v1/getOrders`, `POST /api/v1/createOrder`
2. **Hierarchical Relationships**: Represent sub-resources naturally through nesting:
   - `GET /api/v1/customers/{id}/addresses`
   - Limit nesting depth to 2 levels maximum (`/parent/{id}/child/{id}`). Avoid deep chains like `/a/1/b/2/c/3/d/4`.
3. **HTTP Status Code Mapping**:
   - `200 OK`: Successful read or synchronous update.
   - `201 Created`: Successful creation; MUST include `Location` header pointing to new resource.
   - `202 Accepted`: Asynchronous processing initiated (e.g. batch job).
   - `204 No Content`: Successful mutation with no response body (typical for `DELETE`).
   - `400 Bad Request`: Malformed syntax, invalid JSON, or validation failure.
   - `401 Unauthorized`: Authentication missing or invalid.
   - `403 Forbidden`: Authenticated, but caller lacks necessary permissions.
   - `404 Not Found`: Resource does not exist.
   - `409 Conflict`: Business rule violation, version conflict, or unique constraint duplicate.
   - `422 Unprocessable Content`: Syntactically correct JSON that violates domain rules.

---

## 4. Syntax & Configuration

### Controller Implementation Architecture
```java
@RestController
@RequestMapping(
    value = "/api/v1/orders",
    produces = MediaType.APPLICATION_JSON_VALUE
)
public class OrderResourceController {
    // Endpoints mapped to standard verbs
}
```

---

## 5. Practical Examples

### Example 1: Full RESTful Lifecycle Implementation with Java Records

```java
package com.example.web.rest;

import jakarta.validation.Valid;
import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.Positive;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;
import org.springframework.web.servlet.support.ServletUriComponentsBuilder;

import java.math.BigDecimal;
import java.net.URI;
import java.util.List;
import java.util.UUID;

public record OrderDto(UUID id, String itemDescription, BigDecimal amount, String status) {}
public record CreateOrderRequest(@NotBlank String itemDescription, @Positive BigDecimal amount) {}
public record UpdateOrderRequest(@NotBlank String itemDescription, @Positive BigDecimal amount) {}

@RestController
@RequestMapping("/api/v1/orders")
public class OrderRestController {

    @GetMapping
    public List<OrderDto> listOrders(
            @RequestParam(defaultValue = "0") int page,
            @RequestParam(defaultValue = "20") int size) {
        return List.of(new OrderDto(UUID.randomUUID(), "Monitor Stand", new BigDecimal("49.99"), "PENDING"));
    }

    @GetMapping("/{id}")
    public ResponseEntity<OrderDto> getOrder(@PathVariable UUID id) {
        OrderDto order = new OrderDto(id, "Ergonomic Chair", new BigDecimal("299.99"), "CONFIRMED");
        return ResponseEntity.ok(order);
    }

    @PostMapping
    public ResponseEntity<OrderDto> createOrder(@Valid @RequestBody CreateOrderRequest request) {
        UUID generatedId = UUID.randomUUID();
        OrderDto created = new OrderDto(generatedId, request.itemDescription(), request.amount(), "PENDING");

        // Canonical REST requirement: Location header pointing to the created resource
        URI location = ServletUriComponentsBuilder.fromCurrentRequest()
                .path("/{id}")
                .buildAndExpand(generatedId)
                .toUri();

        return ResponseEntity.created(location).body(created);
    }

    @PutMapping("/{id}")
    public ResponseEntity<OrderDto> replaceOrder(
            @PathVariable UUID id,
            @Valid @RequestBody UpdateOrderRequest request) {
        // Complete state replacement
        OrderDto replaced = new OrderDto(id, request.itemDescription(), request.amount(), "UPDATED");
        return ResponseEntity.ok(replaced);
    }

    @DeleteMapping("/{id}")
    public ResponseEntity<Void> cancelOrder(@PathVariable UUID id) {
        // Idempotent deletion returns 204 No Content
        return ResponseEntity.noContent().build();
    }
}
```

### Example 2: Idempotent API Execution via `Idempotency-Key` Header

In financial transactions, preventing duplicate charges across network retries is paramount.

```java
package com.example.web.idempotency;

import jakarta.servlet.http.HttpServletRequest;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

@RestController
@RequestMapping("/api/v1/payments")
public class IdempotentPaymentController {

    // In production, use Redis with distributed locks and TTL
    private final Map<String, Object> idempotencyCache = new ConcurrentHashMap<>();

    @PostMapping
    public ResponseEntity<?> processPayment(
            @RequestHeader(name = "Idempotency-Key") String idempotencyKey,
            @RequestBody Map<String, Object> paymentPayload) {

        if (idempotencyCache.containsKey(idempotencyKey)) {
            // Replay the exact cached response without re-executing payment!
            return ResponseEntity.status(HttpStatus.OK)
                    .header("X-Cache-Lookup", "HIT")
                    .body(idempotencyCache.get(idempotencyKey));
        }

        // Execute payment transaction
        Map<String, Object> result = Map.of(
                "paymentId", "PAY-" + System.currentTimeMillis(),
                "status", "SETTLED",
                "amount", paymentPayload.get("amount")
        );

        idempotencyCache.put(idempotencyKey, result);

        return ResponseEntity.status(HttpStatus.CREATED)
                .header("X-Cache-Lookup", "MISS")
                .body(result);
    }
}
```

### Example 3: Edge Case — Modeling Non-CRUD State Transitions & RPC Actions

In pure REST, not everything maps to CRUD. Actions like "submitting an order", "cancelling a subscription", or "locking an account" should be modeled idiomatically.

```java
package com.example.web.rest;

import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.util.UUID;

@RestController
@RequestMapping("/api/v1/orders/{orderId}")
public class OrderStateTransitionController {

    /**
     * Idiomatic REST: Subordinate "execution" resource or dedicated sub-path
     */
    @PostMapping("/cancellation")
    public ResponseEntity<String> cancelOrder(@PathVariable UUID orderId,
                                              @RequestBody(required = false) String reason) {
        return ResponseEntity.ok("Order " + orderId + " successfully transitioned to CANCELLED state.");
    }

    /**
     * Alternative: PATCH with state change payload
     */
    @PatchMapping
    public ResponseEntity<String> transitionState(@PathVariable UUID orderId,
                                                  @RequestBody OrderStatePatch patch) {
        return ResponseEntity.ok("State updated to: " + patch.status());
    }
}

record OrderStatePatch(String status, String note) {}
```

### Example 4: Anti-Pattern vs Production Fix

#### Anti-Pattern: HTTP 200 with Embedded Error JSON
```java
// ANTI-PATTERN: Returning 200 OK for business failures
@PostMapping("/pay")
public Map<String, Object> pay(@RequestBody PaymentReq req) {
    if (account.getBalance() < req.getAmount()) {
        // BREAKS HTTP: Client monitoring thinks request succeeded!
        return Map.of("success", false, "error", "Insufficient balance");
    }
    return Map.of("success", true, "txId", "TX123");
}
```

#### Production Fix: Standard HTTP Status Codes and ProblemDetail
```java
// PRODUCTION FIX: Return HTTP 403 Forbidden with RFC 9457 ProblemDetail
@PostMapping("/pay")
public ResponseEntity<?> pay(@RequestBody PaymentReq req) {
    if (account.getBalance() < req.getAmount()) {
        ProblemDetail problem = ProblemDetail.forStatusAndDetail(
                HttpStatus.FORBIDDEN, "Account balance is insufficient for payment.");
        problem.setTitle("Payment Declined");
        return ResponseEntity.status(HttpStatus.FORBIDDEN).body(problem);
    }
    return ResponseEntity.status(HttpStatus.CREATED).body(new PaymentResult("TX123"));
}
```

### Example 5: High-Performance Conditional GET via HTTP `ETag`

```java
package com.example.web.rest;

import org.springframework.http.CacheControl;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;
import org.springframework.web.context.request.WebRequest;

import java.time.Duration;
import java.util.UUID;

public record ProductProfile(UUID id, String name, int revision) {}

@RestController
@RequestMapping("/api/v1/products")
public class ConditionalGetController {

    @GetMapping("/{id}")
    public ResponseEntity<ProductProfile> getProductProfile(@PathVariable UUID id, WebRequest request) {
        ProductProfile product = new ProductProfile(id, "Studio Monitor", 4);
        
        // Compute ETag (Entity Tag) representing current state hash or version
        String etag = "\"rev-" + product.revision() + "\"";

        // If client's If-None-Match matches current ETag, return 304 Not Modified immediately!
        if (request.checkNotModified(etag)) {
            return null; // Spring automatically flushes 304 response with zero body bytes!
        }

        return ResponseEntity.ok()
                .cacheControl(CacheControl.maxAge(Duration.ofMinutes(10)).mustRevalidate())
                .eTag(etag)
                .body(product);
    }
}
```

---

## 6. Best Practices

1. **Use Plural Nouns for Resources**: Consistently name collections using plural nouns (`/users`, `/invoices`, `/orders`).
2. **Always Return `Location` Header on 201**: A `201 Created` response must include the `Location: /api/v1/resources/{newId}` header pointing to the created item.
3. **Never Return 200 OK for Errors**: Use standard HTTP 4xx (client) and 5xx (server) status codes so API gateways, proxies, and observability tools can monitor error rates accurately.
4. **Implement Idempotency Keys for Mutations**: Financial and critical mutation endpoints should accept an `Idempotency-Key` header to safely handle client retries.
5. **Leverage HTTP Caching (`ETag`, `Cache-Control`)**: Use ETags to return `304 Not Modified` on unchanged resources, saving client bandwidth and reducing backend serialization overhead.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Using Verbs in URI Paths** | Breaks REST resource modeling and causes route explosion (`/getUser`, `/deleteUser`). | Use nouns with HTTP verbs (`GET /users/{id}`, `DELETE /users/{id}`). |
| **Returning 200 OK on Validation Errors** | API gateways, load balancers, and monitoring fail to alert on application errors. | Return HTTP 400 Bad Request with RFC 9457 `ProblemDetail`. |
| **Confusing PUT and PATCH** | `PUT` implies replacing the entire resource state; passing partial fields accidentally nulls omitted attributes. | Use `PUT` for complete replacement; use `PATCH` for partial field updates. |
| **Excessive Sub-Resource Nesting** | Deep paths like `/teams/{tId}/projects/{pId}/tasks/{taskId}/comments/{cId}` are fragile and redundant. | Keep nesting to at most 2 levels; access sub-resources directly via their canonical ID. |
| **Ignoring Idempotency on DELETE** | Calling `DELETE /users/1` a second time returning 404 breaks automated client retries. | Ensure multiple DELETE invocations are safe and return 204 No Content. |
| **Exposing Internal Database Primary Keys** | Exposing sequential auto-incrementing IDs allows attackers to enumerate total records and scrape data. | Use UUIDs (UUIDv7 or UUIDv4) or public hash IDs in URI paths. |

---

## 8. Performance & Security Considerations

### Mass Assignment / Over-Posting Vulnerabilities
Never bind request payloads directly onto JPA/Hibernate `@Entity` classes. Attackers can include fields like `isAdmin=true` or `accountBalance=1000000` in the JSON body. Always bind into explicit, read-only DTOs or Java records, copying only validated fields to the domain model.

### Rate Limiting and DoS Prevention
Expose rate limit headers (`X-RateLimit-Limit`, `X-RateLimit-Remaining`, `X-RateLimit-Reset`) to prevent abusive scraping. Return HTTP 429 (`Too Many Requests`) with a `Retry-After` header when clients breach request thresholds.

---

## 9. Comparison: REST vs RPC vs GraphQL

| Dimension | REST (Representational State Transfer) | RPC (gRPC / JSON-RPC) | GraphQL |
| :--- | :--- | :--- | :--- |
| **Core Abstraction** | Resources (Nouns + HTTP Verbs) | Procedures / Functions (Actions) | Graph / Query Document |
| **Network Protocol** | HTTP/1.1, HTTP/2 | HTTP/2 (Binary Protobuf) | HTTP (typically POST only) |
| **Caching** | Native HTTP Caching (CDNs, ETags, Proxies) | Difficult / Client-side only | Difficult (Single POST endpoint) |
| **Over/Under-Fetching** | Can occur without sparse fieldsets | Fixed by schema / proto definition | Eliminated (Client requests exact fields) |
| **Best Used For** | Public APIs, microservice integrations, web | Low-latency internal microservice communication | Mobile apps with complex, heterogeneous data views |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: What is the architectural difference between a PUT and a PATCH request?</strong></summary>

**Answer:**
- **PUT**: Replaces the target resource representation in its entirety. The client sends a complete representation. Any fields omitted in the request body are logically replaced with defaults or nulls. PUT is required to be **idempotent**.
- **PATCH**: Applies a partial set of modifications to the target resource. The client sends only the fields to be updated. Under RFC 5789, PATCH is not strictly required to be idempotent (though most REST API implementations strive to make it idempotent).
</details>

<details>
<summary><strong>Q2: Why is `GET /users/delete?id=5` a dangerous architectural anti-pattern?</strong></summary>

**Answer:**
Under the HTTP specification (RFC 9110), HTTP `GET` must be **safe** and **idempotent**—it must not produce state-changing side effects. Web crawlers (e.g. Googlebot), browser pre-fetching engines, and intermediate proxies pre-fetch GET links automatically. If a GET request deletes a user, automated search engine indexing could wipe out the entire database.
</details>

<details>
<summary><strong>Q3: How do you implement the "Idempotency-Key" pattern in Spring Boot?</strong></summary>

**Answer:**
1. A client provides a unique UUID in the `Idempotency-Key` header on mutating POST requests.
2. A servlet filter or controller intercepts the key and checks a distributed cache (e.g. Redis).
3. If the key exists, the cached previous response body and status code are returned immediately without executing business logic.
4. If absent, a distributed lock is acquired on the key, the business transaction runs, the result is saved in the cache with a TTL (e.g. 24 hours), and the response is returned.
</details>

<details>
<summary><strong>Q4: What is the purpose of the HTTP `Location` header in REST APIs?</strong></summary>

**Answer:**
When an endpoint creates a new resource and returns HTTP `201 Created`, RFC 9110 mandates supplying the `Location` header containing the absolute or relative URI of the newly created resource (e.g., `Location: /api/v1/orders/ORD-9912`). This enables clients to navigate directly to the new entity.
</details>

<details>
<summary><strong>Q5: How does HTTP conditional evaluation with `ETag` and `If-None-Match` work in Spring MVC?</strong></summary>

**Answer:**
The server calculates an `ETag` (a hash or version string) representing the resource state and returns it in the response header. On subsequent requests, the client sends this value in `If-None-Match`. In Spring MVC, `webRequest.checkNotModified(etag)` compares the values; if they match, Spring immediately short-circuits the request and returns HTTP `304 Not Modified` with an empty response body, saving bandwidth and processing.
</details>

<details>
<summary><strong>Q6: Why should internal database auto-incrementing IDs never be exposed in public API URLs?</strong></summary>

**Answer:**
1. **Enumeration Attacks**: Sequential IDs (`/users/1`, `/users/2`) allow attackers to easily iterate and scrape all records.
2. **Business Intelligence Leakage**: Sequential IDs reveal growth rates (e.g., difference between `order/1000` on Monday and `order/1500` on Friday reveals order volume).
3. Use UUIDs (such as UUIDv7) or opaque public IDs instead.
</details>

<details>
<summary><strong>Q7: What is the difference between HTTP status 401 Unauthorized and 403 Forbidden?</strong></summary>

**Answer:**
- **401 Unauthorized**: Means "Unauthenticated". The client has not supplied valid credentials.
- **403 Forbidden**: The client's identity is authenticated and known, but the client does not possess the required permissions or roles to perform the requested operation.
</details>

<details>
<summary><strong>Q8: What is the difference between 400 Bad Request and 422 Unprocessable Content?</strong></summary>

**Answer:**
- **400 Bad Request**: Malformed syntax (e.g., unparseable JSON, invalid characters, wrong query parameter types).
- **422 Unprocessable Content**: Syntactically valid JSON that violates semantic domain rules (e.g., end date precedes start date, credit limit exceeded).
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Create 201 Created Response with Location Header</strong></summary>

**Task**: Write an endpoint `/api/v1/items` that creates an item, assigns UUID `11111111-1111-1111-1111-111111111111`, and returns HTTP 201 with a `Location` header.

```java
// Solution:
@PostMapping("/api/v1/items")
public ResponseEntity<Void> createItem(@RequestBody ItemDto dto) {
    UUID id = UUID.fromString("11111111-1111-1111-1111-111111111111");
    URI location = ServletUriComponentsBuilder.fromCurrentRequest()
            .path("/{id}")
            .buildAndExpand(id)
            .toUri();
    return ResponseEntity.created(location).build();
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Idempotent Delete Endpoint</strong></summary>

**Task**: Implement an idempotent `DELETE /api/v1/items/{id}` that returns HTTP 204 No Content.

```java
// Solution:
@DeleteMapping("/api/v1/items/{id}")
public ResponseEntity<Void> deleteItem(@PathVariable UUID id) {
    // Delete operation idempotently cleans item if present
    return ResponseEntity.noContent().build();
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Conditional GET with `ETag` Validation</strong></summary>

**Task**: Write a controller endpoint returning a user profile that checks an ETag based on `user.version()`, returning 304 if unchanged.

```java
// Solution:
@GetMapping("/api/v1/users/{id}")
public ResponseEntity<UserDto> getUser(@PathVariable UUID id, WebRequest webRequest) {
    UserDto user = new UserDto(id, "Alice", 5);
    String etag = "\"v" + user.version() + "\"";
    if (webRequest.checkNotModified(etag)) {
        return null; // Emits 304 Not Modified
    }
    return ResponseEntity.ok().eTag(etag).body(user);
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Sub-Resource Hierarchical Controller</strong></summary>

**Task**: Create an endpoint to list all comments for an article at `GET /api/v1/articles/{articleId}/comments`.

```java
// Solution:
@RestController
@RequestMapping("/api/v1/articles/{articleId}/comments")
public class ArticleCommentController {
    @GetMapping
    public List<CommentDto> listComments(@PathVariable UUID articleId) {
        return List.of(new CommentDto(UUID.randomUUID(), articleId, "Great read!"));
    }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Distributed Idempotency Interceptor via Redis</strong></summary>

**Task**: Build a `HandlerInterceptor` that intercepts requests containing an `Idempotency-Key` header, queries Redis using an atomic `SETNX` lock, and returns the cached response if a duplicate request arrives.
</details>

<details>
<summary><strong>Exercise 6 (Hard): JSON Merge Patch (RFC 7396) Processor</strong></summary>

**Task**: Implement a generic `@PatchMapping` controller endpoint that parses an incoming `application/merge-patch+json` payload using Jackson's `JsonMergePatch` and applies changes to an existing domain record.
</details>

---

## 12. Summary & Quick Reference

- **Resources**: Plural nouns (`/orders`, `/customers`).
- **HTTP Verbs**: GET (read), POST (create), PUT (replace), PATCH (modify), DELETE (remove).
- **Status Codes**: 200 (OK), 201 (Created + Location), 204 (No Content), 400 (Bad Request), 404 (Not Found), 409 (Conflict).
- **Errors**: RFC 9457 `ProblemDetail` with `application/problem+json`.
- **Golden Rule**: Never return HTTP 200 for application error conditions.

---

## 13. Further Reading

- [Architectural Styles and the Design of Network-based Software Architectures (Roy Fielding)](https://www.ics.uci.edu/~fielding/pubs/dissertation/top.htm)
- [RFC 9110: HTTP Semantics](https://www.rfc-editor.org/rfc/rfc9110.html)
- [Richardson Maturity Model (Martin Fowler)](https://martinfowler.com/articles/richardsonMaturityModel.html)
- [Microsoft REST API Guidelines](https://github.com/microsoft/api-guidelines)
