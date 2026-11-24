# HATEOAS & Rich REST in Spring Boot

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | REST API Design Best Practices, Spring MVC Controllers, HAL Specifications |
| **Estimated Reading Time** | 24 minutes |
| **Status** | Production-Ready |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | OpenJDK 21 LTS / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | Spring HATEOAS 2.4, OpenJDK 21.0.2 |

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
9. [Comparison: Standard REST vs HATEOAS / HAL](#9-comparison-standard-rest-vs-hateoas--hal)
10. [Interview Questions & Deep Dives](#10-interview-questions--deep-dives)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

According to Roy Fielding, an API is not truly RESTful unless it adheres to the hypermedia constraint: **Hypermedia As The Engine Of Application State (HATEOAS)** (Richardson Maturity Level 3).

### The Problem
In standard "Level 2" REST APIs:
1. Clients must hardcode URI templates (e.g. `https://api.example.com/orders/{id}/cancel`).
2. If the server reorganizes its URL scheme, all mobile apps, web frontends, and external client integrations break immediately.
3. Clients must duplicate backend business state machines (e.g., guessing whether an order can currently be cancelled or refunded based on local boolean flags).

### The Solution: Hypermedia-Driven APIs
With HATEOAS, a client navigates the REST service dynamically through hypermedia links (`_links`) provided directly in the resource payload. Just as a human navigates a website by following HTML `<a>` tags without memorizing every URL, a client program follows hypermedia links. The presence or absence of a link indicates whether a state transition is currently permitted. Spring Boot integrates `spring-boot-starter-hateoas`, providing models (`EntityModel`, `CollectionModel`, `PagedModel`) and link builders (`WebMvcLinkBuilder`) to build discoverable APIs.

---

## 2. Core Concepts

### Mental Model & The HAL (Hypertext Application Language) Format
```
           Client receives Order Representation:
           GET /api/v1/orders/ORD-101
                     │
                     ▼
{
  "orderId": "ORD-101",
  "status": "PAID",
  "totalAmount": 129.50,
  "_links": {
    "self": {
      "href": "https://api.example.com/api/v1/orders/ORD-101"
    },
    "customer": {
      "href": "https://api.example.com/api/v1/customers/CUST-88"
    },
    "cancel": {
      "href": "https://api.example.com/api/v1/orders/ORD-101/cancellation"
    },
    "invoice": {
      "href": "https://api.example.com/api/v1/orders/ORD-101/invoice"
    }
  }
}
```

### Core Representation Models in Spring HATEOAS
- **`RepresentationModel`**: Base class holding a collection of `Link` instances.
- **`EntityModel<T>`**: Wraps a single domain object `T` and decorates it with `_links`.
- **`CollectionModel<T>`**: Wraps a collection of items, providing top-level collection links (e.g., pagination, self).
- **`PagedModel<T>`**: Specialized collection model decorated with pagination metadata (`size`, `totalElements`, `totalPages`, `number`) and navigation links (`next`, `prev`, `first`, `last`).

---

## 3. How It Works Internally

### Type-Safe Link Construction with `WebMvcLinkBuilder`
Instead of hardcoding URI strings, Spring HATEOAS inspects controller classes and methods using reflection and CGLIB proxies:

```java
// Dynamically builds: /api/v1/orders/{orderId}
Link selfLink = WebMvcLinkBuilder.linkTo(
    WebMvcLinkBuilder.methodOn(OrderController.class).getOrder(orderId)
).withSelfRel();
```

1. `methodOn(...)` creates a dynamic proxy of the controller class.
2. Invoking the controller method records the target method metadata, path patterns (`@GetMapping`), and parameter values.
3. `linkTo(...)` examines the current servlet context, reverse-proxy headers (`X-Forwarded-Host`, `X-Forwarded-Proto`), and resolves the absolute URI.

---

## 4. Syntax & Configuration

### Dependency Setup (`pom.xml`)
```xml
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-hateoas</artifactId>
</dependency>
```

### Response Content-Type
By default, Spring HATEOAS negotiates media type:
- `application/hal+json` (HAL JSON)
- `application/json` (fallback)

---

## 5. Practical Examples

### Example 1: Basic `EntityModel` with Self and Association Links

```java
package com.example.web.hateoas.controller;

import org.springframework.hateoas.EntityModel;
import org.springframework.hateoas.Link;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.math.BigDecimal;
import java.util.UUID;

import static org.springframework.hateoas.server.mvc.WebMvcLinkBuilder.*;

public record CustomerDto(UUID id, String name, String email) {}

@RestController
@RequestMapping("/api/v1/customers")
public class CustomerHateoasController {

    @GetMapping("/{id}")
    public ResponseEntity<EntityModel<CustomerDto>> getCustomer(@PathVariable UUID id) {
        CustomerDto customer = new CustomerDto(id, "Alice Smith", "alice@example.com");

        // Construct dynamic type-safe links
        Link selfLink = linkTo(methodOn(CustomerHateoasController.class).getCustomer(id)).withSelfRel();
        Link ordersLink = linkTo(methodOn(CustomerHateoasController.class).getCustomerOrders(id)).withRel("orders");

        EntityModel<CustomerDto> model = EntityModel.of(customer, selfLink, ordersLink);
        return ResponseEntity.ok(model);
    }

    @GetMapping("/{id}/orders")
    public String getCustomerOrders(@PathVariable UUID id) {
        return "Orders for customer " + id;
    }
}
```

### Example 2: Enterprise State Machine Transition Links via `RepresentationModelAssembler`

Using `RepresentationModelAssembler` decouples hypermedia link generation from the controller, encapsulating business rules determining which actions are valid.

```java
package com.example.web.hateoas.assembler;

import com.example.web.hateoas.controller.OrderController;
import org.springframework.hateoas.EntityModel;
import org.springframework.hateoas.server.RepresentationModelAssembler;
import org.springframework.stereotype.Component;

import java.math.BigDecimal;
import java.util.UUID;

import static org.springframework.hateoas.server.mvc.WebMvcLinkBuilder.*;

public enum OrderState { CREATED, PAID, SHIPPED, CANCELLED }

public record OrderDomainRecord(UUID id, BigDecimal amount, OrderState state) {}

@Component
public class OrderModelAssembler implements RepresentationModelAssembler<OrderDomainRecord, EntityModel<OrderDomainRecord>> {

    @Override
    public EntityModel<OrderDomainRecord> toModel(OrderDomainRecord order) {
        // Unconditional self link
        EntityModel<OrderDomainRecord> model = EntityModel.of(
                order,
                linkTo(methodOn(OrderController.class).getOrder(order.id())).withSelfRel()
        );

        // Conditional hypermedia links based on business state machine:
        if (order.state() == OrderState.CREATED) {
            model.add(linkTo(methodOn(OrderController.class).payOrder(order.id())).withRel("payment"));
            model.add(linkTo(methodOn(OrderController.class).cancelOrder(order.id())).withRel("cancel"));
        } else if (order.state() == OrderState.PAID) {
            model.add(linkTo(methodOn(OrderController.class).shipOrder(order.id())).withRel("ship"));
            model.add(linkTo(methodOn(OrderController.class).refundOrder(order.id())).withRel("refund"));
        }
        // If SHIPPED or CANCELLED, neither payment nor cancel links are provided!

        return model;
    }
}
```

### Example 3: Edge Case — Handling Reverse-Proxy URLs (X-Forwarded Headers)

When running behind API Gateways, Kubernetes Ingress, or AWS ALBs, `WebMvcLinkBuilder` must generate links pointing to the public gateway URL (e.g. `https://gateway.company.com`) rather than the internal container hostname (`http://10.244.0.15:8080`).

```yaml
# application.yaml
server:
  forward-headers-strategy: framework # Tells Spring to process X-Forwarded-Host/Proto headers!
```

```java
package com.example.web.hateoas.config;

import org.springframework.context.annotation.Configuration;
import org.springframework.web.servlet.config.annotation.WebMvcConfigurer;

/**
 * When server.forward-headers-strategy=framework is set,
 * Spring's ForwardedHeaderFilter inspects X-Forwarded-Host, X-Forwarded-Proto,
 * and X-Forwarded-Port, ensuring all linkTo() calls output public customer-facing URIs.
 */
@Configuration(proxyBeanMethods = false)
public class ForwardedHeadersConfig implements WebMvcConfigurer {
}
```

### Example 4: Anti-Pattern vs Production Fix

#### Anti-Pattern: Hardcoded URI String Concatenation
```java
// ANTI-PATTERN: Brittle, hardcoded String link generation
@GetMapping("/{id}")
public Map<String, Object> getBadOrder(@PathVariable String id) {
    Map<String, Object> response = new HashMap<>();
    response.put("id", id);
    // Hardcoded host and path! Breaks if scheme changes, port changes, or route is renamed
    response.put("_links", Map.of(
        "self", "http://localhost:8080/api/v1/orders/" + id
    ));
    return response;
}
```

#### Production Fix: Type-Safe `WebMvcLinkBuilder`
```java
// PRODUCTION FIX: Refactoring-safe, context-aware link generation
@GetMapping("/{id}")
public EntityModel<OrderDto> getGoodOrder(@PathVariable UUID id) {
    OrderDto dto = orderService.findById(id);
    return EntityModel.of(dto,
        linkTo(methodOn(OrderController.class).getOrder(id)).withSelfRel()
    );
}
```

### Example 5: High-Performance Paged Model with HAL Navigation

```java
package com.example.web.hateoas.controller;

import org.springframework.data.domain.Page;
import org.springframework.data.domain.PageImpl;
import org.springframework.data.domain.PageRequest;
import org.springframework.hateoas.PagedModel;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.util.List;
import java.util.UUID;

import static org.springframework.hateoas.server.mvc.WebMvcLinkBuilder.*;

public record ItemSummary(UUID id, String title) {}

@RestController
@RequestMapping("/api/v1/catalog")
public class CatalogHateoasController {

    @GetMapping
    public ResponseEntity<PagedModel<ItemSummary>> getCatalogPage(
            @RequestParam(defaultValue = "0") int page,
            @RequestParam(defaultValue = "10") int size) {

        List<ItemSummary> items = List.of(
                new ItemSummary(UUID.randomUUID(), "Ergonomic Desk"),
                new ItemSummary(UUID.randomUUID(), "Mechanical Keyboard")
        );
        Page<ItemSummary> pagedResult = new PageImpl<>(items, PageRequest.of(page, size), 50);

        PagedModel.PageMetadata metadata = new PagedModel.PageMetadata(
                pagedResult.getSize(),
                pagedResult.getNumber(),
                pagedResult.getTotalElements(),
                pagedResult.getTotalPages()
        );

        PagedModel<ItemSummary> pagedModel = PagedModel.of(
                pagedResult.getContent(),
                metadata,
                linkTo(methodOn(CatalogHateoasController.class).getCatalogPage(page, size)).withSelfRel(),
                linkTo(methodOn(CatalogHateoasController.class).getCatalogPage(page + 1, size)).withRel("next"),
                linkTo(methodOn(CatalogHateoasController.class).getCatalogPage(0, size)).withRel("first"),
                linkTo(methodOn(CatalogHateoasController.class).getCatalogPage(pagedResult.getTotalPages() - 1, size)).withRel("last")
        );

        return ResponseEntity.ok(pagedModel);
    }
}
```

---

## 6. Best Practices

1. **Use `RepresentationModelAssembler`**: Encapsulate hypermedia link creation inside dedicated assembler components rather than littering controllers with `linkTo()` logic.
2. **Set `server.forward-headers-strategy=framework`**: Ensure links generated behind reverse proxies and API gateways point to external HTTPS domains rather than internal pod IPs.
3. **Use Standard Relation Names (`IanaLinkRelations`)**: Favor standard relations (`self`, `next`, `prev`, `first`, `last`, `item`, `collection`) over custom names whenever possible.
4. **Drive Client Behavior via Link Presence**: Teach clients to inspect `_links` rather than hardcoding state machines. If `_links.cancel` exists, show the "Cancel Order" button; if it's missing, disable or hide the button.
5. **Support Content Negotiation**: Support both `application/hal+json` and standard `application/json` so legacy clients that don't recognize HAL can still consume the API.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Hardcoding Hostnames or Ports in Links** | Breaks when deployed to staging, production, or behind ingress load balancers. | Always construct links via `WebMvcLinkBuilder.linkTo()`. |
| **Omitting `server.forward-headers-strategy`** | Ingress translates HTTPS to internal HTTP; links generated have `http://` instead of `https://`. | Configure `server.forward-headers-strategy: framework`. |
| **Returning Raw Entities Instead of Models** | Domain entities lack hypermedia metadata, resulting in standard Level 2 JSON. | Wrap domain models in `EntityModel<T>` or `CollectionModel<T>`. |
| **Leaking Inapplicable Action Links** | Sending a `cancel` link on an already `SHIPPED` order causes clients to attempt impossible state transitions. | Check entity state before adding action links in the assembler. |
| **High Overhead in Method Proxies** | Creating hundreds of `methodOn()` proxies in tight loops inside large lists degrades CPU performance. | Build template links or reuse static relative URIs for large collections. |
| **Missing `_embedded` Collections in HAL** | In HAL, collection items must reside under `_embedded`. Manual serialization fails HAL schema checks. | Use `CollectionModel<T>` or `PagedModel<T>`. |

---

## 8. Performance & Security Considerations

### Method Proxy CGLIB Overhead
`WebMvcLinkBuilder.methodOn()` uses CGLIB reflection to intercept method calls. In endpoints returning thousands of records, generating links individually for every single row in a loop can degrade serialization throughput. For massive result sets, generate links only at the collection level, or use Keyset Pagination without per-item hypermedia.

### Information Disclosure via Links
Hypermedia links expose internal endpoints. Ensure that links returned in payloads match the caller's authorized permissions. For example, do not emit an `admin-audit` link if the requesting user lacks `ROLE_ADMIN`.

---

## 9. Comparison: Standard REST vs HATEOAS / HAL

| Feature | Level 2 Standard REST | Level 3 HATEOAS (HAL) |
| :--- | :--- | :--- |
| **URI Discovery** | Client hardcodes URI templates | Client discovers URIs dynamically from `_links` |
| **Client-Server Coupling** | High (Client tightly coupled to URL schema) | Low (Client coupled only to relation names) |
| **State Machine Governance** | Duplicated across client and server | Driven entirely by server link presence |
| **Payload Format** | Plain JSON (`application/json`) | HAL JSON (`application/hal+json`) |
| **Bandwidth Overhead** | Minimal | Slight increase due to link metadata |
| **Primary Use Cases** | Internal private microservices | Public partner APIs, long-term evolving APIs |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: What does HATEOAS stand for and why is it considered the highest level of REST maturity?</strong></summary>

**Answer:**
HATEOAS stands for **Hypermedia As The Engine Of Application State**. In Roy Fielding's definition of REST, a client interacts with a network application entirely through hypermedia provided dynamically by application servers. This decouples the client from hardcoded URLs and allows the server to evolve URI schemes and state transitions independently without breaking existing client deployments.
</details>

<details>
<summary><strong>Q2: How does `WebMvcLinkBuilder` know the base URL of the application when constructing links?</strong></summary>

**Answer:**
`WebMvcLinkBuilder` accesses `ServletUriComponentsBuilder.fromCurrentServletMapping()`, which inspects the active `HttpServletRequest` via `RequestContextHolder`. It extracts the scheme, server name, port, and servlet context path. When `server.forward-headers-strategy=framework` is enabled, it automatically reads `X-Forwarded-Proto`, `X-Forwarded-Host`, and `X-Forwarded-Port` headers.
</details>

<details>
<summary><strong>Q3: What is the HAL (Hypertext Application Language) specification?</strong></summary>

**Answer:**
HAL is an IETF draft specification defining a standard convention for expressing hypermedia links and embedded resources in JSON. It reserves two top-level JSON keys:
1. `_links`: Contains hypermedia links mapped by link relation names (`self`, `next`, custom relations).
2. `_embedded`: Contains nested representations of related resources (e.g., list of items in a collection).
</details>

<details>
<summary><strong>Q4: What is the role of `RepresentationModelAssembler` in Spring HATEOAS?</strong></summary>

**Answer:**
`RepresentationModelAssembler<T, D extends RepresentationModel<?>>` is a design pattern interface that converts a domain model `T` into a hypermedia-aware model `D` (such as `EntityModel<T>`). It encapsulates all link generation, conditional state checks, and route mappings in a single reusable bean, preventing controller clutter.
</details>

<details>
<summary><strong>Q5: How can a client discover available actions on a resource using HATEOAS?</strong></summary>

**Answer:**
By inspecting the `_links` object returned in the resource representation. For example, if an order object contains a `cancel` link (`_links.cancel.href`), the client knows cancellation is valid. If the order transitions to `SHIPPED`, the server omits the `cancel` link, signaling to the client that the cancel action is no longer permitted.
</details>

<details>
<summary><strong>Q6: How does Spring HATEOAS handle templated URIs (URI variables)?</strong></summary>

**Answer:**
Spring HATEOAS supports RFC 6570 URI Templates. You can construct templated links using `Link.of("/api/users{?name,page}", "search")`. In HAL JSON, this generates `"templated": true` on the link object, indicating to the client that variables can be substituted.
</details>

<details>
<summary><strong>Q7: What is the difference between `EntityModel` and `CollectionModel`?</strong></summary>

**Answer:**
- `EntityModel<T>`: Represents a single resource entity decorated with links.
- `CollectionModel<T>`: Represents an array/collection of resources decorated with links, serializing items under `_embedded` in HAL format.
</details>

<details>
<summary><strong>Q8: Can HATEOAS links be internationalized or localized?</strong></summary>

**Answer:**
Yes. Links can include `hreflang` attributes indicating target resource language representations, and link titles (`title` attribute) can be resolved dynamically via Spring's `MessageSource` based on the client's `Accept-Language` header.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Add Self Link to DTO</strong></summary>

**Task**: Wrap a record `BookDto(UUID id, String title)` inside an `EntityModel` and add a self link.

```java
// Solution:
@GetMapping("/books/{id}")
public EntityModel<BookDto> getBook(@PathVariable UUID id) {
    BookDto book = new BookDto(id, "Clean Code");
    Link selfLink = linkTo(methodOn(BookController.class).getBook(id)).withSelfRel();
    return EntityModel.of(book, selfLink);
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Configure Forwarded Headers Strategy</strong></summary>

**Task**: Write the `application.yaml` property enabling Spring Framework to handle `X-Forwarded-*` headers for reverse proxies.

```yaml
# Solution:
server:
  forward-headers-strategy: framework
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Model Assembler for User Resource</strong></summary>

**Task**: Implement a `RepresentationModelAssembler` for a `User` entity that adds `self` and `roles` links.

```java
// Solution:
@Component
public class UserModelAssembler implements RepresentationModelAssembler<User, EntityModel<User>> {
    @Override
    public EntityModel<User> toModel(User user) {
        return EntityModel.of(user,
                linkTo(methodOn(UserController.class).getUser(user.getId())).withSelfRel(),
                linkTo(methodOn(UserController.class).getUserRoles(user.getId())).withRel("roles"));
    }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Conditional Transition Links</strong></summary>

**Task**: In an order assembler, add a link with relation `approve` only if `order.isPendingApproval()` returns true.

```java
// Solution:
@Override
public EntityModel<Order> toModel(Order order) {
    EntityModel<Order> model = EntityModel.of(order,
            linkTo(methodOn(OrderController.class).getOrder(order.getId())).withSelfRel());
    if (order.isPendingApproval()) {
        model.add(linkTo(methodOn(OrderController.class).approveOrder(order.getId())).withRel("approve"));
    }
    return model;
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Affordances and IANA Form Relations</strong></summary>

**Task**: Use Spring HATEOAS **Affordances** to declare input payload expectations on a `PUT` link attached to an `EntityModel`, enabling clients to discover acceptable schema inputs.
</details>

<details>
<summary><strong>Exercise 6 (Hard): Custom HalHandlerConfig for Vendor Media Types</strong></summary>

**Task**: Configure Spring HATEOAS to serialize HAL representations when clients request a custom vendor media type: `application/vnd.company.v1+hal+json`.
</details>

---

## 12. Summary & Quick Reference

- **HATEOAS**: Richardson Maturity Level 3; clients navigate state via `_links`.
- **Core Wrapper**: `EntityModel<T>` for objects, `CollectionModel<T>` for lists.
- **Link Builder**: `WebMvcLinkBuilder.linkTo(methodOn(...))`.
- **Assemblers**: Use `RepresentationModelAssembler` to isolate link logic.
- **Proxy Invariant**: Set `server.forward-headers-strategy: framework`.

---

## 13. Further Reading

- [Spring HATEOAS Official Reference Documentation](https://docs.spring.io/spring-hateoas/docs/current/reference/html/)
- [IETF HAL Specification (JSON Hypertext Application Language)](https://datatracker.ietf.org/doc/html/draft-kelly-json-hal)
- [Richardson Maturity Model by Martin Fowler](https://martinfowler.com/articles/richardsonMaturityModel.html)
- [IANA Official Link Relations Registry](https://www.iana.org/assignments/link-relations/link-relations.xhtml)
