# API Versioning Strategies in Spring Boot

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | Spring MVC Request Mapping, HTTP Headers, Content Negotiation |
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
9. [Comparison: The Four Major Versioning Strategies](#9-comparison-the-four-major-versioning-strategies)
10. [Interview Questions & Deep Dives](#10-interview-questions--deep-dives)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

APIs change over time. As business requirements evolve, developers must rename fields, modify validation constraints, restructure response schemas, and decommission deprecated behaviors. In public APIs and distributed microservices, breaking existing client integrations is unacceptable.

### The Problem
Without an intentional API versioning strategy:
1. Field name changes break existing mobile applications whose users do not update immediately.
2. Codebases accumulate spaghetti `if-else` branches trying to accommodate old and new formats simultaneously.
3. Teams duplicate entire controller codebases, multiplying maintenance costs.

### The Solution: Explicit API Versioning
API versioning allows services to deploy new capabilities while maintaining strict backward compatibility for existing consumers. In Spring Boot, versioning can be implemented using four primary strategies:
1. **URI Path Versioning** (`/api/v1/orders` vs `/api/v2/orders`)
2. **HTTP Header Versioning** (`X-API-Version: 2`)
3. **Query Parameter Versioning** (`/api/orders?version=2`)
4. **Media Type / Content Negotiation Versioning** (`Accept: application/vnd.company.v2+json`)

---

## 2. Core Concepts

### Mental Model: The Four Versioning Strategies
```
1. URI Path Versioning:
   GET /api/v1/customers/42   ──► CustomerControllerV1.getCustomer()
   GET /api/v2/customers/42   ──► CustomerControllerV2.getCustomer()

2. Query Parameter Versioning:
   GET /api/customers/42?v=1  ──► @GetMapping(params = "v=1")
   GET /api/customers/42?v=2  ──► @GetMapping(params = "v=2")

3. Request Header Versioning:
   GET /api/customers/42      ──► @GetMapping(headers = "X-API-Version=1")
   Header: X-API-Version=1
   GET /api/customers/42      ──► @GetMapping(headers = "X-API-Version=2")
   Header: X-API-Version=2

4. Media Type (Content Negotiation) Versioning:
   GET /api/customers/42      ──► @GetMapping(produces = "application/vnd.company.v1+json")
   Accept: application/vnd.company.v1+json
   GET /api/customers/42      ──► @GetMapping(produces = "application/vnd.company.v2+json")
   Accept: application/vnd.company.v2+json
```

### Invariants
1. **Additive Changes Do Not Require Versioning**: Adding a new optional field or a new endpoint is non-breaking and does **not** warrant incrementing the major API version.
2. **Breaking Changes Demand Versioning**: Renaming fields, changing data types, removing fields, or altering error codes are breaking changes that require a new version.
3. **Sunset and Deprecation Policies**: Old versions must declare a deprecation timeline using standard `Sunset` and `Deprecation` HTTP headers (RFC 8594).

---

## 3. How It Works Internally

### RequestMapping Matching Matrix
Spring's `RequestMappingHandlerMapping` matches incoming HTTP requests using multiple criteria simultaneously:
- **`path`**: Matches URI path patterns.
- **`params`**: Matches query parameter values.
- **`headers`**: Matches arbitrary HTTP request headers.
- **`produces`**: Matches the `Accept` header.

When two controller methods share the same path (e.g. `/api/customers`), Spring differentiates between them based on `headers`, `params`, or `produces` attributes. If no matching condition is found, Spring returns HTTP 404 (or HTTP 406 for media type mismatches).

---

## 4. Syntax & Configuration

### Controller Method Mapping Attributes
```java
// Header Versioning
@GetMapping(value = "/customers/{id}", headers = "X-API-Version=1")
public CustomerV1 getCustomerV1(@PathVariable UUID id) { ... }

@GetMapping(value = "/customers/{id}", headers = "X-API-Version=2")
public CustomerV2 getCustomerV2(@PathVariable UUID id) { ... }

// Media Type Versioning
@GetMapping(value = "/customers/{id}", produces = "application/vnd.acme.v1+json")
public CustomerV1 getCustomerV1Media(@PathVariable UUID id) { ... }

@GetMapping(value = "/customers/{id}", produces = "application/vnd.acme.v2+json")
public CustomerV2 getCustomerV2Media(@PathVariable UUID id) { ... }
```

---

## 5. Practical Examples

### Example 1: URI Path Versioning with Controller Packages

```java
// Package: com.example.web.v1
package com.example.web.v1;

import org.springframework.web.bind.annotation.*;
import java.util.UUID;

public record UserResponseV1(UUID id, String fullName) {}

@RestController
@RequestMapping("/api/v1/users")
public class UserV1Controller {

    @GetMapping("/{id}")
    public UserResponseV1 getUser(@PathVariable UUID id) {
        return new UserResponseV1(id, "John Doe");
    }
}
```

```java
// Package: com.example.web.v2
package com.example.web.v2;

import org.springframework.web.bind.annotation.*;
import java.util.UUID;

// V2 introduces separate firstName and lastName fields (breaking schema change)
public record UserResponseV2(UUID id, String firstName, String lastName, String locale) {}

@RestController
@RequestMapping("/api/v2/users")
public class UserV2Controller {

    @GetMapping("/{id}")
    public UserResponseV2 getUser(@PathVariable UUID id) {
        return new UserResponseV2(id, "John", "Doe", "en-US");
    }
}
```

### Example 2: Header-Based Versioning with Standard Deprecation Headers (RFC 8594)

```java
package com.example.web.header;

import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.time.Instant;
import java.util.UUID;

public record OrderV1(UUID id, double price) {}
public record OrderV2(UUID id, String formattedPrice, String currency) {}

@RestController
@RequestMapping("/api/orders")
public class HeaderVersionedOrderController {

    // V1: Deprecated, includes Sunset header warning callers of decommission date
    @GetMapping(value = "/{id}", headers = "X-API-Version=1")
    public ResponseEntity<OrderV1> getOrderV1(@PathVariable UUID id) {
        return ResponseEntity.ok()
                .header("Deprecation", "true")
                .header("Sunset", "Wed, 31 Dec 2026 23:59:59 GMT") // RFC 8594 Sunset Header
                .header("Link", "<https://api.example.com/docs/v2-migration>; rel=\"deprecation\"")
                .body(new OrderV1(id, 99.95));
    }

    // V2: Active modern version
    @GetMapping(value = "/{id}", headers = "X-API-Version=2")
    public ResponseEntity<OrderV2> getOrderV2(@PathVariable UUID id) {
        return ResponseEntity.ok(new OrderV2(id, "$99.95", "USD"));
    }
}
```

### Example 3: Edge Case — Media Type Versioning with Fallback to Default Version

When clients do not send a vendor `Accept` header, the service should gracefully fall back to a default version rather than throwing HTTP 406.

```java
package com.example.web.mediatype;

import org.springframework.http.MediaType;
import org.springframework.web.bind.annotation.*;

import java.util.UUID;

public record ProductV1(UUID id, String name) {}
public record ProductV2(UUID id, String name, String sku) {}

@RestController
@RequestMapping("/api/products")
public class MediaTypeVersionedController {

    // Matches vendor V1 header
    @GetMapping(value = "/{id}", produces = "application/vnd.company.v1+json")
    public ProductV1 getProductV1(@PathVariable UUID id) {
        return new ProductV1(id, "Mechanical Keyboard");
    }

    // Matches vendor V2 header
    @GetMapping(value = "/{id}", produces = "application/vnd.company.v2+json")
    public ProductV2 getProductV2(@PathVariable UUID id) {
        return new ProductV2(id, "Mechanical Keyboard", "KB-MK-88");
    }

    // Fallback: Default application/json maps to latest stable version (V2)
    @GetMapping(value = "/{id}", produces = MediaType.APPLICATION_JSON_VALUE)
    public ProductV2 getDefaultProduct(@PathVariable UUID id) {
        return getProductV2(id);
    }
}
```

### Example 4: Anti-Pattern vs Production Fix

#### Anti-Pattern: In-Method Version Branching
```java
// ANTI-PATTERN: Polluting business code with branching logic based on headers
@GetMapping("/users/{id}")
public Object getUserBad(@PathVariable UUID id, @RequestHeader(name = "X-Version", defaultValue = "1") int v) {
    if (v == 1) {
        return new UserV1(id, "John Doe");
    } else if (v == 2) {
        return new UserV2(id, "John", "Doe");
    } else {
        throw new IllegalArgumentException("Unknown version: " + v);
    }
}
```

#### Production Fix: Declarative Handler Separation
```java
// PRODUCTION FIX: Clean, isolated controller methods with declarative routing
@GetMapping(value = "/users/{id}", headers = "X-Version=1")
public UserV1 getUserV1(@PathVariable UUID id) {
    return userService.getV1(id);
}

@GetMapping(value = "/users/{id}", headers = "X-Version=2")
public UserV2 getUserV2(@PathVariable UUID id) {
    return userService.getV2(id);
}
```

### Example 5: High-Performance Custom Annotation-Driven Versioning Engine

```java
package com.example.web.versioning;

import org.springframework.core.annotation.AliasFor;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestMethod;

import java.lang.annotation.*;

/**
 * Meta-annotation binding methods to custom API versions using headers.
 */
@Target({ElementType.METHOD, ElementType.TYPE})
@Retention(RetentionPolicy.RUNTIME)
@Documented
@RequestMapping(method = RequestMethod.GET)
public @interface ApiVersion {
    int value();

    @AliasFor(annotation = RequestMapping.class, attribute = "headers")
    String[] headers() default {};
}
```

---

## 6. Best Practices

1. **Favor URI Path Versioning for Public APIs**: URI path versioning (`/api/v1/`) is explicit, transparent in browser addresses, works seamlessly with CDNs, and is universally understood by all clients.
2. **Favor Media Type Versioning for Strict REST APIs**: If adherence to Roy Fielding's strict HATEOAS principle (where URIs identify resources, not representations) is required, use content negotiation (`Accept: application/vnd.company.v1+json`).
3. **Avoid In-Method Branching**: Do not write `if (version == 1)` inside your service or controller methods. Split versions into distinct controller classes or methods.
4. **Broadcast Decommissioning with RFC 8594**: Always attach `Deprecation: true` and `Sunset: <date>` HTTP headers when responding to requests on deprecated versions.
5. **Decouple Internal Domain from Versioned DTOs**: Never bind database entities directly to versioned endpoints. Maintain version-specific DTOs and map them to internal domain entities.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Bumping Version for Non-Breaking Additions** | Forces clients to upgrade unnecessarily when adding a new optional field. | Treat additions as backward-compatible; do not bump major version. |
| **Missing Default Version Fallback** | Unversioned requests return HTTP 404 or 406, breaking unversioned exploratory clients. | Provide a default route mapping to the current stable version. |
| **Breaking Caching with Header Versioning** | CDNs and reverse proxies cache responses by URL; clients requesting V2 receive cached V1 responses. | Emit `Vary: X-API-Version` or `Vary: Accept` headers to prevent cache collisions. |
| **Supporting Indefinite Legacy Versions** | Maintaining 10+ legacy versions causes technical debt and security maintenance nightmare. | Enforce an explicit deprecation and decommissioning policy (e.g. N-2 support). |
| **Mixing Versioning Strategies** | Using URL versioning on some routes and header versioning on others confuses client teams. | Standardize on a single versioning strategy across the organization. |
| **Ambiguous Mapping Conflicts** | Defining identical paths without distinct header/param qualifiers causes startup failure with `IllegalStateException`. | Verify each method has mutually exclusive matching conditions. |

---

## 8. Performance & Security Considerations

### Cache Collisions in Intermediate Proxies
When using Header or Media Type versioning, intermediate caching proxies (Cloudflare, Fastly, NGINX) cache responses by default using the URI path alone. If user A requests `/items/1` with version 1, and user B requests `/items/1` with version 2, user B may receive the cached version 1 response! To prevent this, always include `Vary: X-API-Version` or `Vary: Accept` in response headers.

### Unmaintained Legacy Version Vulnerabilities
Old API versions often contain outdated validation logic or bypass newer security filters. Regularly audit older endpoints for security vulnerabilities and enforce decommissioning schedules.

---

## 9. Comparison: The Four Major Versioning Strategies

| Strategy | Example | CDN Cache Friendly? | URL Cleanliness | Industry Adoption |
| :--- | :--- | :--- | :--- | :--- |
| **URI Path** | `/api/v1/users` | Yes (Out of the box) | Changes URI per version | Highest (Stripe, Twitter, Google) |
| **Query Parameter** | `/api/users?version=1` | Yes (if configured) | Cluttered URLs | Moderate (Amazon AWS, internal APIs) |
| **Custom Header** | `X-API-Version: 1` | Requires `Vary` header | Clean URIs | Moderate (Microsoft, enterprise SaaS) |
| **Media Type / Accept** | `Accept: application/vnd.v1+json` | Requires `Vary` header | Purest REST semantics | Moderate (GitHub) |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: Why is URI path versioning (`/v1/users`) often criticized by strict REST purists?</strong></summary>

**Answer:**
Under Roy Fielding's REST architectural style, a URI should identify a unique **resource** (a concept in domain space), not a specific **representation** of that resource. Changing the URI (`/v1/users` vs `/v2/users`) implies that they are two distinct resources, whereas in reality, they are different representations of the same underlying resource. Strict REST purists argue that representation format changes should be negotiated via the `Accept` header. However, URI versioning remains the most widely adopted due to its simplicity and CDN cache friendliness.
</details>

<details>
<summary><strong>Q2: What is the purpose of the RFC 8594 `Sunset` header?</strong></summary>

**Answer:**
The `Sunset` HTTP response header communicates to clients that an API endpoint or version is deprecated and scheduled for decommissioning at a specific future timestamp. Its value is an HTTP-date (e.g. `Sunset: Wed, 31 Dec 2026 23:59:59 GMT`). API gateways and automated client monitors inspect this header to alert engineering teams of impending breaking changes.
</details>

<details>
<summary><strong>Q3: How does Spring MVC prevent ambiguous mapping errors when multiple methods map to the same URI?</strong></summary>

**Answer:**
`RequestMappingHandlerMapping` evaluates differentiating attributes such as `params`, `headers`, `consumes`, and `produces`. As long as the conditions are mutually exclusive (e.g. one method has `headers = "X-Version=1"` and another has `headers = "X-Version=2"`), Spring treats them as distinct routes without ambiguity.
</details>

<details>
<summary><strong>Q4: Why must you configure the `Vary` header when using header-based API versioning?</strong></summary>

**Answer:**
HTTP proxies and CDNs use the URI as the primary cache key. If two clients request the same URL with different `X-API-Version` headers, a proxy might serve the cached version 1 response to the client requesting version 2. The `Vary: X-API-Version` header instructs the proxy to incorporate the version header into its cache key lookup.
</details>

<details>
<summary><strong>Q5: What changes are considered "non-breaking" in an API contract?</strong></summary>

**Answer:**
Non-breaking changes include:
1. Adding a new optional request field or query parameter.
2. Adding a new field to a response body (assuming clients follow the Tolerant Reader pattern).
3. Adding a new endpoint.
4. Adding a new allowed HTTP verb on an existing resource.
</details>

<details>
<summary><strong>Q6: How do you structure a multi-version Spring Boot project to prevent circular dependencies?</strong></summary>

**Answer:**
Organize packages by version (`com.example.api.v1` and `com.example.api.v2`). Each package contains its own version-specific DTOs and controllers. Both versions inject shared underlying domain service interfaces or use dedicated adapters, keeping controllers decoupled from one another.
</details>

<details>
<summary><strong>Q7: Can you combine URL versioning with query parameter versioning in Spring Boot?</strong></summary>

**Answer:**
Yes. You can specify `@RequestMapping(value = "/api/v1/orders", params = "stage=beta")` to handle beta features within a major version.
</details>

<details>
<summary><strong>Q8: What status code should be returned if a client requests an unsupported API version?</strong></summary>

**Answer:**
- For header/query parameter versioning: HTTP 400 (Bad Request) with `ProblemDetail` explaining the version is invalid or unsupported.
- For media type versioning: HTTP 406 (Not Acceptable).
- For URI path versioning: HTTP 404 (Not Found) or 410 (Gone) if the version was retired.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Query Parameter Versioned Controller</strong></summary>

**Task**: Write two `@GetMapping` endpoints for `/api/status`: one matching `?v=1` returning `"STATUS_V1"`, and one matching `?v=2` returning `"STATUS_V2"`.

```java
// Solution:
@RestController
@RequestMapping("/api/status")
public class StatusController {
    @GetMapping(params = "v=1")
    public String getV1() { return "STATUS_V1"; }

    @GetMapping(params = "v=2")
    public String getV2() { return "STATUS_V2"; }
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Sunset Header Decorator</strong></summary>

**Task**: Write a controller method returning an entity along with standard RFC 8594 `Deprecation` and `Sunset` headers.

```java
// Solution:
@GetMapping("/api/v1/legacy")
public ResponseEntity<String> getLegacy() {
    return ResponseEntity.ok()
            .header("Deprecation", "true")
            .header("Sunset", "Fri, 01 Jan 2027 00:00:00 GMT")
            .body("Legacy Payload");
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Media Type Versioned Controller</strong></summary>

**Task**: Create an endpoint that produces `application/vnd.mycorp.v1+json` returning `RecordV1(String name)` and `application/vnd.mycorp.v2+json` returning `RecordV2(String firstName, String lastName)`.

```java
// Solution:
public record RecordV1(String name) {}
public record RecordV2(String firstName, String lastName) {}

@RestController
@RequestMapping("/api/profiles")
public class ProfileController {
    @GetMapping(produces = "application/vnd.mycorp.v1+json")
    public RecordV1 getV1() { return new RecordV1("John Doe"); }

    @GetMapping(produces = "application/vnd.mycorp.v2+json")
    public RecordV2 getV2() { return new RecordV2("John", "Doe"); }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Custom Filter Appending Vary Header</strong></summary>

**Task**: Implement a `OncePerRequestFilter` that appends `Vary: X-API-Version` to every outgoing response.

```java
// Solution:
@Component
public class VaryVersionHeaderFilter extends OncePerRequestFilter {
    @Override
    protected void doFilterInternal(HttpServletRequest request, HttpServletResponse response, FilterChain filterChain)
            throws ServletException, IOException {
        response.addHeader("Vary", "X-API-Version");
        filterChain.doFilter(request, response);
    }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Custom HandlerMapping with Semantic Versioning (SemVer)</strong></summary>

**Task**: Implement a custom `RequestCondition` and `RequestMappingHandlerMapping` that evaluates semantic version ranges in headers (e.g., `X-API-Version: >=2.1.0 <3.0.0`) and routes to the highest compatible handler method.
</details>

<details>
<summary><strong>Exercise 6 (Hard): Automated Sunset Deprecation Interceptor</strong></summary>

**Task**: Build a `HandlerInterceptor` that inspects an `@ApiDeprecated(sunset = "2026-12-31")` annotation on controller methods and automatically injects standard `Deprecation` and `Sunset` headers into outgoing responses.
</details>

---

## 12. Summary & Quick Reference

- **URI Versioning**: `/api/v1/`—most practical, cache-friendly, and universally adopted.
- **Media Type Versioning**: `Accept: application/vnd.app.v1+json`—purest REST design.
- **Header Versioning**: `X-API-Version: 1`—requires explicit `Vary` headers for caching.
- **Deprecation**: Use RFC 8594 `Deprecation: true` and `Sunset: <date>` headers.
- **Golden Rule**: Never bump major API versions for additive, backward-compatible changes.

---

## 13. Further Reading

- [RFC 8594: The Sunset HTTP Header Field](https://www.rfc-editor.org/rfc/rfc8594.html)
- [RFC 9110: HTTP Semantics and Content Negotiation](https://www.rfc-editor.org/rfc/rfc9110.html)
- [Microsoft REST API Guidelines: Versioning](https://github.com/microsoft/api-guidelines/blob/vNext/Guidelines.md#12-versioning)
- [Spring Framework RequestMapping Handler Mapping Reference](https://docs.spring.io/spring-framework/reference/web/webmvc/mvc-controller/ann-requestmapping.html)
