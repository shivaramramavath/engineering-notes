# API Versioning Strategies in Spring Boot

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.x |
| **Tested With** | Spring Web MVC, OpenAPI 3.0 / Swagger, Jackson |

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
10. [Interview Questions & Deep Dives](#10-interview-questions--deep-dives)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### The Problem
Public and internal REST APIs are living contracts. Over time, business requirements change: fields must be deprecated, response structures must be restructured, and data types must be modified. If an engineering team introduces breaking changes directly to an active endpoint, thousands of third-party consumers, mobile apps, and frontend clients immediately crash. Conversely, refusing to evolve an API causes technical stagnation.

### The Evolution
To evolve APIs without breaking active clients, software architects employ **API Versioning Strategies**. Spring Boot supports four distinct approaches:
1. **URI Path Versioning**: `/api/v1/orders` vs `/api/v2/orders` (Most popular, explicit).
2. **Request Parameter Versioning**: `/api/orders?version=2` (Simple, query-driven).
3. **Custom Request Header Versioning**: `X-API-Version: 2` (Keeps URIs clean).
4. **Media Type / Content Negotiation (Accept Header)**: `Accept: application/vnd.company.app-v2+json` (Strict REST/HATEOAS compliant).

### Production Relevance
Selecting the right versioning strategy is crucial for API Gateways, CDN caching policies, client SDK generation, and enterprise developer experience (DX). Standardizing on a clear versioning model allows teams to run multiple versions in parallel during transition periods and gracefully sunset legacy contracts.

---

## 2. Core Concepts

### Comparison of the 4 Versioning Strategies

```
+-----------------------------------------------------------------------------------------+
|                               API VERSIONING STRATEGIES                                 |
+-----------------------------------------------------------------------------------------+
|                                                                                         |
|  1. URI PATH VERSIONING                                                                 |
|     GET /api/v1/customers/101                                                           |
|     GET /api/v2/customers/101                                                           |
|     - Pros: Explicit, easy to test in browser, cache-friendly for CDNs                  |
|     - Cons: Violates pure REST URI permanence principle                                 |
|                                                                                         |
|  2. QUERY PARAMETER VERSIONING                                                          |
|     GET /api/customers/101?v=1                                                          |
|     GET /api/customers/101?v=2                                                          |
|     - Pros: Easy to add to existing endpoints, default fallback when omitted           |
|     - Cons: Can interfere with other query parameters; routing can become cluttered     |
|                                                                                         |
|  3. CUSTOM REQUEST HEADER VERSIONING                                                    |
|     GET /api/customers/101                                                              |
|     Header: "X-API-Version: 2"                                                          |
|     - Pros: Clean, uniform URIs; easy for programmatic HTTP clients                     |
|     - Cons: Cannot test directly in browser address bar; requires CDN header caching    |
|                                                                                         |
|  4. MEDIA TYPE / CONTENT NEGOTIATION ("Accept" Header)                                  |
|     GET /api/customers/101                                                              |
|     Header: "Accept: application/vnd.example.customer.v2+json"                          |
|     - Pros: Strict REST compliance (Content Negotiation); version is a representation   |
|     - Cons: High cognitive overhead; complex curl and documentation examples            |
|                                                                                         |
+-----------------------------------------------------------------------------------------+
```

### Architectural Invariants
- **Non-Breaking Changes Require No New Version**: Adding a new optional field or an additional endpoint should be backward-compatible; do not bump the major version.
- **Graceful Deprecation**: Legacy API versions must return standard HTTP deprecation headers (`Sunset: Wed, 11 Nov 2026 00:00:00 GMT` and `Deprecation: @1762819200`) before decommissioning.
- **Parallel Run Support**: The backend must support running Version 1 and Version 2 simultaneously without duplicating business logic.

---

## 3. How It Works Internally

### Spring MVC Handler Mapping Resolution

```
[ Incoming HTTP Request ]
           |
           v
[ DispatcherServlet ]
           |
           v
[ RequestMappingHandlerMapping ]
   Evaluates all registered RequestMappingInfo conditions:
   1. PatternsCondition: matches URI ("/api/v1/users" vs "/api/v2/users")
   2. ParamsCondition: matches query param ("v=1" vs "v=2")
   3. HeadersCondition: matches header ("X-API-Version=1" vs "2")
   4. ConsumesCondition / ProducesCondition: matches Accept header
           |
           v
   Selects Controller method with the most specific match
           |
           v
[ UserV2Controller.getUser() ]
           |
           v
Emits V2 DTO Response
```

Spring MVC's `RequestMappingHandlerMapping` constructs a composite `RequestMappingInfo` for every controller method. When a request arrives, Spring compares:
- Path matchers (URI path strategy).
- Header matchers (`headers = "X-API-Version=2"`).
- Produces matchers (`produces = "application/vnd.company.v2+json"`).
If no version is specified by the client, Spring can route to a default fallback method.

---

## 4. Syntax & Usage

### 1. Strategy 1: URI Path Versioning

```java
package com.example.versioning.uri;

import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

// Version 1 Controller
@RestController
@RequestMapping("/api/v1/customers")
public class CustomerV1Controller {

    public record CustomerV1Dto(Long id, String fullName) {}

    @GetMapping("/{id}")
    public CustomerV1Dto getCustomer(@PathVariable Long id) {
        return new CustomerV1Dto(id, "John Doe");
    }
}

// Version 2 Controller
@RestController
@RequestMapping("/api/v2/customers")
public class CustomerV2Controller {

    public record CustomerV2Dto(Long id, String firstName, String lastName) {}

    @GetMapping("/{id}")
    public CustomerV2Dto getCustomer(@PathVariable Long id) {
        return new CustomerV2Dto(id, "John", "Doe");
    }
}
```

### 2. Strategy 3: Custom Header Versioning

```java
package com.example.versioning.header;

import org.springframework.web.bind.annotation.*;

@RestController
@RequestMapping("/api/orders")
public class OrderHeaderController {

    // Matches when header X-API-Version is "1" (or default)
    @GetMapping(value = "/{id}", headers = "X-API-Version=1")
    public OrderV1Dto getOrderV1(@PathVariable Long id) {
        return new OrderV1Dto(id, "ORDER_V1");
    }

    // Matches when header X-API-Version is "2"
    @GetMapping(value = "/{id}", headers = "X-API-Version=2")
    public OrderV2Dto getOrderV2(@PathVariable Long id) {
        return new OrderV2Dto(id, "ORDER_V2", true);
    }
}
```

---

## 5. Practical Examples

### Example 1: Strategy 4 - Media Type / Content Negotiation (Accept Header)

```java
package com.example.versioning.mediatype;

import org.springframework.web.bind.annotation.*;

@RestController
@RequestMapping("/api/products")
public class ProductContentNegotiationController {

    public record ProductV1(Long id, String name, double price) {}
    public record ProductV2(Long id, String name, String currency, double price) {}

    // curl -H "Accept: application/vnd.company.product.v1+json" http://localhost:8080/api/products/1
    @GetMapping(value = "/{id}", produces = "application/vnd.company.product.v1+json")
    public ProductV1 getProductV1(@PathVariable Long id) {
        return new ProductV1(id, "Mechanical Keyboard", 129.99);
    }

    // curl -H "Accept: application/vnd.company.product.v2+json" http://localhost:8080/api/products/1
    @GetMapping(value = "/{id}", produces = "application/vnd.company.product.v2+json")
    public ProductV2 getProductV2(@PathVariable Long id) {
        return new ProductV2(id, "Mechanical Keyboard", "USD", 129.99);
    }
}
```

### Example 2: Emitting Standard HTTP Sunset / Deprecation Headers for Legacy Versions

When an API version is deprecated, RFC 8594 dictates providing `Deprecation` and `Sunset` headers informing client engineers when the version will be permanently shut down.

```java
package com.example.versioning.deprecation;

import jakarta.servlet.http.HttpServletResponse;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

@RestController
@RequestMapping("/api/v1/legacy-data")
public class DeprecatedApiController {

    @GetMapping
    public ResponseEntity<String> getLegacyData(HttpServletResponse response) {
        return ResponseEntity.ok()
                // RFC 8594: Indicates API is deprecated
                .header("Deprecation", "@1762819200") // Unix timestamp
                // RFC 8594: Date when endpoint will return 410 Gone / 404
                .header("Sunset", "Sun, 01 Nov 2026 00:00:00 GMT")
                .header("Link", "<https://api.example.com/docs/migration-v2>; rel=\"deprecation\"")
                .body("Legacy payload data");
    }
}
```

### Example 3: Shared Core Business Service Supporting Multiple DTO Versions

```java
package com.example.versioning.service;

import org.springframework.stereotype.Service;

// Single core business model
public record UserAccount(Long id, String givenName, String familyName, String email) {}

@Service
public class UserService {
    public UserAccount findById(Long id) {
        return new UserAccount(id, "Ada", "Lovelace", "ada@example.com");
    }
}

// Controller delegating to common service while transforming DTOs
@RestController
public class VersionTransformController {

    private final UserService userService;
    public VersionTransformController(UserService userService) { this.userService = userService; }

    // V1 Representation
    @GetMapping("/api/v1/users/{id}")
    public UserV1Response getUserV1(@PathVariable Long id) {
        UserAccount account = userService.findById(id);
        return new UserV1Response(account.id(), account.givenName() + " " + account.familyName());
    }

    // V2 Representation
    @GetMapping("/api/v2/users/{id}")
    public UserV2Response getUserV2(@PathVariable Long id) {
        UserAccount account = userService.findById(id);
        return new UserV2Response(account.id(), account.givenName(), account.familyName(), account.email());
    }
}
```

### Example 4: Anti-Pattern (Modifying Live Active API Contracts in Place) vs Versioning

#### The Anti-Pattern: Breaking Live Mobile Clients
```java
// BAD: Renaming a property directly on a shared DTO in an active production endpoint!
// Original V1 DTO: public record CustomerDto(String fullName) {}
// Dev changes it in-place to:
public record CustomerDto(String firstName, String lastName) {} // BREAKING CHANGE!

// Mobile app version 1.2 crashes on startup because it expects "fullName" in the JSON!
@GetMapping("/api/customers/{id}")
public CustomerDto getCustomer(@PathVariable Long id) {
    return new CustomerDto("Grace", "Hopper");
}
```

#### Production Fix: Explicit URI Versioning with Backward Compatibility
```java
// GOOD: Leave V1 completely untouched! Introduce V2 for new clients.
@GetMapping("/api/v1/customers/{id}")
public CustomerV1Dto getCustomerV1(@PathVariable Long id) {
    return new CustomerV1Dto("Grace Hopper"); // Mobile v1.2 continues working!
}

@GetMapping("/api/v2/customers/{id}")
public CustomerV2Dto getCustomerV2(@PathVariable Long id) {
    return new CustomerV2Dto("Grace", "Hopper"); // Web & Mobile v2.0 uses new contract
}
```

### Example 5: Custom HandlerMapping for Semantic Versioning (SemVer)

```java
package com.example.versioning.custom;

import org.springframework.web.servlet.mvc.condition.HeadersRequestCondition;
import org.springframework.web.servlet.mvc.method.RequestMappingInfo;
import org.springframework.web.servlet.mvc.method.annotation.RequestMappingHandlerMapping;

import java.lang.reflect.Method;

public class SemVerRequestMappingHandlerMapping extends RequestMappingHandlerMapping {

    @Override
    protected RequestMappingInfo getMappingForMethod(Method method, Class<?> handlerType) {
        RequestMappingInfo info = super.getMappingForMethod(method, handlerType);
        if (info == null) return null;

        ApiVersion apiVersion = method.getAnnotation(ApiVersion.class);
        if (apiVersion == null) {
            apiVersion = handlerType.getAnnotation(ApiVersion.class);
        }

        if (apiVersion != null) {
            // Automatically appends header check "X-API-Version=X.Y"
            HeadersRequestCondition condition = new HeadersRequestCondition("X-API-Version=" + apiVersion.value());
            info = info.combine(new RequestMappingInfo(null, null, null, condition, null, null, null));
        }

        return info;
    }
}
```

---

## 6. Best Practices

1. **Prefer URI Path Versioning for Public APIs**: URI Path versioning (`/api/v1/...`) is universally understood, trivial to test in web browsers, and works seamlessly with standard CDN caches (Cloudflare, CloudFront).
2. **Never Break an Active API Contract**: Additive changes (new optional query params, new fields in response JSON) are non-breaking. Renaming fields, changing data types, or deleting fields require a new API version.
3. **Decouple Versioned DTOs from Business Models**: Do not duplicate core business services for each version. Keep one internal business domain model and map to version-specific DTO records (`CustomerV1Dto`, `CustomerV2Dto`).
4. **Emit RFC 8594 Deprecation and Sunset Headers**: Warn API consumers at least 6 months before permanently taking down a deprecated version.
5. **Support Maximum of 2 to 3 Active Versions Concurrently**: Running 10 historical API versions simultaneously creates severe maintenance overhead. Establish a formal deprecation schedule.
6. **Configure OpenAPI (Swagger) to Document All Active Versions**: Provide a version selector dropdown in Swagger UI using Springdoc-openapi `GroupedOpenApi`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| **Bumping Versions for Additive Changes** | Creating `/api/v2` just because you added a new `middleName` field forces clients to migrate unnecessarily. | Additive optional fields do not require a major version bump. |
| **Duplicating Database Tables per API Version** | Creating `orders_v1` and `orders_v2` tables fractures transactional integrity and storage. | Keep database tables unified; transform data representation exclusively in the presentation/DTO layer. |
| **CDN Cache Pollution with Header Versioning** | CDNs cache based on URI by default; if Header versioning is used without `Vary: X-API-Version`, users receive wrong cached versions! | Add `Vary: X-API-Version` response header, or stick to URI path versioning. |
| **Abruptly Terminating Legacy Versions** | Deleting V1 endpoints without notice breaks third-party customer integrations, causing high-severity customer complaints. | Provide 6-month deprecation notice with automated warning emails to registered API key owners. |
| **Inconsistent Versioning Across Endpoints** | Versioning some endpoints via URI (`/v1/users`) and others via headers (`X-Version: 2`) confuses developers. | Standardize on one versioning strategy across the entire organization. |
| **Forgetting to Update Integration Test Suites** | Testing only the newest V2 endpoints allows regressions to break V1 endpoints silently. | Maintain automated contract tests (e.g., Spring Cloud Contract) for all active versions. |

---

## 8. Performance & Security Notes

### Performance
- **CDN Edge Caching**: URI Path versioning allows edge caches (Cloudflare, Akamai) to cache responses permanently by URI key without inspecting HTTP request headers.
- **Handler Mapping Overhead**: Having 200 versioned endpoints has negligible routing impact in Spring MVC ($O(1)$ hash map lookup in `RequestMappingHandlerMapping`).

### Security
- **Deprecation Scanning**: Security scanners (OWASP ZAP) frequently discover unpatched vulnerabilities in forgotten legacy API versions (`/v1/`). Ensure security patches and authentication filters apply equally across all active versions.
- **Rate Limiting by Version**: Apply stricter rate limits to deprecated API versions to incentivize third-party developers to upgrade to newer versions.

---

## 9. Comparison / When to Use What

| Strategy | Example Request | Browser Testable | CDN Cache Friendly | REST Purity |
|---|---|---|---|---|
| **URI Path** | `GET /api/v1/orders` | Yes (Easy) | Yes (Default URI Key) | Low (URI encodes version) |
| **Query Param** | `GET /api/orders?v=1` | Yes | Moderate (Depends on query cache)| Low |
| **Custom Header** | `Header: X-API-Version: 1` | No (Requires curl/Postman)| Requires `Vary` Header | Moderate |
| **Media Type (Accept)**| `Accept: app/vnd.v1+json` | No | Requires `Vary: Accept` | Highest (True REST HATEOAS)|

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>1. Why is URI Path Versioning the industry standard despite purist REST objections?</strong></summary>

Strict REST purists argue that a URI should identify a unique resource (`/orders/123`), not its representation format.
However, URI path versioning (`/api/v1/orders/123`) has become the universal industry standard (used by Google, Stripe, GitHub, AWS) because:
1. **Developer Experience (DX)**: Trivial to copy/paste into a browser, documentation, or Slack.
2. **Caching**: Edge proxies and CDNs cache directly on the URI without complex `Vary` header configurations.
3. **API Gateway Routing**: Ingress controllers (Nginx, Envoy, Kong) can easily route `/v1/` traffic to legacy clusters and `/v2/` traffic to modern clusters using simple path-based routing rules.
</details>

<details>
<summary><strong>2. What is the purpose of the RFC 8594 `Sunset` and `Deprecation` HTTP headers?</strong></summary>

RFC 8594 standardizes how HTTP APIs signal that a resource or version is nearing end-of-life:
- **`Deprecation`**: A boolean flag or timestamp indicating that the endpoint should no longer be used for new development:
  `Deprecation: true` OR `Deprecation: @1762819200`
- **`Sunset`**: An immutable date and time after which the server will permanently decommission the endpoint (returning 410 Gone or 404 Not Found):
  `Sunset: Wed, 11 Nov 2026 00:00:00 GMT`
- **`Link`**: Points to human-readable migration instructions:
  `Link: <https://api.example.com/docs/v2-migration>; rel="deprecation"`
</details>

<details>
<summary><strong>3. How does Spring MVC route requests when Header Versioning is used?</strong></summary>

In Spring MVC, the `@RequestMapping` annotation accepts a `headers` attribute:
```java
@GetMapping(value = "/users", headers = "X-Version=1")
public UserV1 getUserV1() {}

@GetMapping(value = "/users", headers = "X-Version=2")
public UserV2 getUserV2() {}
```
Spring's `RequestMappingHandlerMapping` checks the incoming request headers. If `X-Version` equals `"1"`, it invokes `getUserV1()`. If no matching header condition is met, Spring throws `HttpRequestMethodNotSupportedException` or falls back to an unconstrained default handler.
</details>

<details>
<summary><strong>4. What constitutes a "breaking change" versus a "non-breaking change" in an API contract?</strong></summary>

- **Non-Breaking (Additive)**:
  - Adding a new optional query parameter or request body field.
  - Adding a new field to a JSON response payload (assuming clients follow Postel's Law / ignore unknown properties).
  - Adding an entirely new endpoint (`POST /api/orders/bulk`).
- **Breaking**:
  - Renaming or deleting an existing field in a request or response.
  - Changing a field's data type (e.g., from `Long` to `String`).
  - Adding a new mandatory (required) request parameter or header.
  - Changing the semantics or behavior of an existing endpoint.
</details>

<details>
<summary><strong>5. How do you configure Springdoc-OpenAPI (Swagger) to document multiple API versions?</strong></summary>

By defining multiple `GroupedOpenApi` beans in a `@Configuration` class:
```java
@Bean
public GroupedOpenApi v1Api() {
    return GroupedOpenApi.builder()
            .group("v1")
            .pathsToMatch("/api/v1/**")
            .build();
}

@Bean
public GroupedOpenApi v2Api() {
    return GroupedOpenApi.builder()
            .group("v2")
            .pathsToMatch("/api/v2/**")
            .build();
}
```
Swagger UI will display a dropdown menu enabling developers to view OpenAPI specs for V1 and V2 independently.
</details>

<details>
<summary><strong>6. Why is it dangerous to duplicate database tables for different API versions?</strong></summary>

Duplicating database tables (`orders_v1` vs `orders_v2`):
- Causes split-brain state: if a customer creates an order on V1 and attempts to pay on V2, the record cannot be found.
- Requires complex, error-prone two-way data synchronization.
- Storage and index maintenance costs double.
**Rule**: Keep one source of truth in the database; adapt schemas using versioned DTOs in the application presentation layer.
</details>

<details>
<summary><strong>7. How does Content Negotiation versioning affect HTTP caching?</strong></summary>

When using `Accept: application/vnd.company.v1+json`, the URI remains identical (`/api/users/1`) across all versions.
If an intermediary cache (like Squid or Cloudflare) caches the response for V1, a subsequent request with `Accept: application/vnd.company.v2+json` might erroneously receive the cached V1 payload!
**Remedy**: The server must always return the HTTP header:
`Vary: Accept`
This instructs caches to index cached responses by both the URI and the `Accept` header.
</details>

<details>
<summary><strong>8. What is Postel's Law (The Robustness Principle) and how does it apply to API versioning?</strong></summary>

"Be conservative in what you do, be liberal in what you accept from others."
In API design:
- API producers should be strict about formatting their output.
- API consumers should configure their JSON parsers (Jackson `FAIL_ON_UNKNOWN_PROPERTIES=false`) to ignore unexpected new fields.
This allows backend teams to add new fields without forcing a major version bump.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Implement URI-Versioned Customer Endpoints</strong></summary>

**Objective**: Create two endpoints under `/api/v1/profile` and `/api/v2/profile` returning distinct DTO records.

**Solution**:
```java
public record ProfileV1Dto(String name) {}
public record ProfileV2Dto(String firstName, String lastName, String avatarUrl) {}

@RestController
public class ProfileController {

    @GetMapping("/api/v1/profile")
    public ProfileV1Dto getV1() {
        return new ProfileV1Dto("Linus Torvalds");
    }

    @GetMapping("/api/v2/profile")
    public ProfileV2Dto getV2() {
        return new ProfileV2Dto("Linus", "Torvalds", "https://cdn.example.com/linus.png");
    }
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Implement Query-Parameter Based Versioning</strong></summary>

**Objective**: Use the `params` attribute of `@GetMapping` to route requests based on `version=1` vs `version=2`.

**Solution**:
```java
@RestController
@RequestMapping("/api/settings")
public class SettingsController {

    @GetMapping(params = "version=1")
    public String getSettingsV1() {
        return "Settings V1: {theme: 'DARK'}";
    }

    @GetMapping(params = "version=2")
    public String getSettingsV2() {
        return "Settings V2: {theme: 'DARK', compactMode: true}";
    }
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Add Automated Sunset and Deprecation Interceptor</strong></summary>

**Objective**: Write a `HandlerInterceptor` that automatically appends `Sunset` and `Deprecation` headers to any endpoint starting with `/api/v1/`.

**Solution**:
```java
@Component
public class DeprecationWarningInterceptor implements HandlerInterceptor {

    @Override
    public boolean preHandle(HttpServletRequest request, HttpServletResponse response, Object handler) {
        if (request.getRequestURI().startsWith("/api/v1/")) {
            response.setHeader("Deprecation", "@1762819200");
            response.setHeader("Sunset", "Fri, 01 Jan 2027 00:00:00 GMT");
            response.setHeader("Link", "<https://api.example.com/docs/upgrade-guide>; rel=\"deprecation\"");
        }
        return true;
    }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Configure GroupedOpenApi for Multiple Versions</strong></summary>

**Objective**: Write a Springdoc `@Configuration` class declaring Swagger documentation groups for `/api/v1/**` and `/api/v2/**`.

**Solution**:
```java
@Configuration
public class OpenApiConfig {

    @Bean
    public GroupedOpenApi publicApiV1() {
        return GroupedOpenApi.builder()
                .group("v1-legacy")
                .pathsToMatch("/api/v1/**")
                .build();
    }

    @Bean
    public GroupedOpenApi publicApiV2() {
        return GroupedOpenApi.builder()
                .group("v2-active")
                .pathsToMatch("/api/v2/**")
                .build();
    }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Implement Custom Annotation `@ApiVersion` with Path Prefix Rewriter</strong></summary>

**Objective**: Create a custom `@ApiVersion(1)` annotation that automatically prefixes the class mapping with `/api/v{version}` without hardcoding it in `@RequestMapping`.

**Solution**:
```java
@Target({ElementType.TYPE, ElementType.METHOD})
@Retention(RetentionPolicy.RUNTIME)
public @interface ApiVersion {
    int value();
}

@Configuration
public class WebMvcVersioningConfig implements WebMvcRegistrations {

    @Override
    public RequestMappingHandlerMapping getRequestMappingHandlerMapping() {
        return new RequestMappingHandlerMapping() {
            @Override
            protected void registerHandlerMethod(Object handler, Method method, RequestMappingInfo mapping) {
                Class<?> beanType = method.getDeclaringClass();
                ApiVersion apiVersion = beanType.getAnnotation(ApiVersion.class);
                if (apiVersion != null) {
                    RequestMappingInfo prefix = RequestMappingInfo.paths("/api/v" + apiVersion.value()).build();
                    mapping = prefix.combine(mapping);
                }
                super.registerHandlerMethod(handler, method, mapping);
            }
        };
    }
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Integration Test Asserting Content Negotiation Version Routing</strong></summary>

**Objective**: Write a `MockMvc` integration test asserting that requests with `Accept: application/vnd.company.v1+json` and `v2+json` receive different response structures.

**Solution**:
```java
@WebMvcTest(controllers = ProductContentNegotiationController.class)
class VersionNegotiationTest {

    @Autowired private MockMvc mockMvc;

    @Test
    void shouldRouteToV1WhenAcceptHeaderMatches() throws Exception {
        mockMvc.perform(get("/api/products/1")
                .accept("application/vnd.company.product.v1+json"))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.price").value(129.99))
                .andExpect(jsonPath("$.currency").doesNotExist());
    }

    @Test
    void shouldRouteToV2WhenAcceptHeaderMatches() throws Exception {
        mockMvc.perform(get("/api/products/1")
                .accept("application/vnd.company.product.v2+json"))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.price").value(129.99))
                .andExpect(jsonPath("$.currency").value("USD"));
    }
}
```
</details>

---

## 12. Summary & Cheat Sheet

```
+----------------------------------------------------------------------------------------------------+
| API VERSIONING IN SPRING BOOT CHEAT SHEET                                                          |
+----------------------------------------------------------------------------------------------------+
| Strategy            | Syntax Example                            | Primary Recommendation           |
+---------------------+-------------------------------------------+----------------------------------+
| URI Path            | @RequestMapping("/api/v1/orders")         | Recommended default for public   |
|                     |                                           | APIs; CDN cache friendly         |
+---------------------+-------------------------------------------+----------------------------------+
| Custom Header       | @GetMapping(headers = "X-API-Version=2")  | Clean URIs; requires             |
|                     |                                           | 'Vary: X-API-Version' header     |
+---------------------+-------------------------------------------+----------------------------------+
| Content Negotiation | @GetMapping(produces = "app/vnd.v2+json") | Strict REST compliance;          |
| (Accept Header)     |                                           | requires 'Vary: Accept' header   |
+---------------------+-------------------------------------------+----------------------------------+
| Query Parameter     | @GetMapping(params = "version=2")         | Ad-hoc testing; less formal      |
+---------------------+-------------------------------------------+----------------------------------+
| Sunset / Deprecate  | Headers: "Deprecation", "Sunset", "Link"  | RFC 8594 standard for warning    |
|                     |                                           | clients before decommissioning   |
+----------------------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [RFC 8594: Sunset HTTP Header Field](https://datatracker.ietf.org/doc/html/rfc8594)
- [Microsoft REST API Guidelines: Versioning](https://github.com/microsoft/api-guidelines/blob/vNext/Guidelines.md#12-versioning)
- [Spring Framework Reference: RequestMapping Conditions](https://docs.spring.io/spring-framework/reference/web/webmvc/mvc-controller/ann-requestmapping.html)
- [Springdoc-OpenAPI Multi-Version Documentation Guide](https://springdoc.org/#how-can-i-display-different-versions-in-swagger-ui)
- [Martin Fowler on Backward Compatibility and Versioning](https://martinfowler.com/articles/enterpriseREST.html)
