# Layered Architecture in Spring Boot

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.x |
| **Tested With** | JUnit 5, Mockito, Spring Data JPA, ArchUnit |

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
When building backend services without structured architectural boundaries, code naturally degenerates into a tangled "Big Ball of Mud". Web controllers begin executing raw SQL statements, business validation rules are scattered across database stored procedures and presentation view templates, and persistence entities leak directly into external HTTP APIs. This lack of encapsulation makes unit testing impossible, introduces brittle side effects, and prevents swapping infrastructure components.

### The Evolution
The **Classic 3-Tier Layered Architecture** (Presentation $\to$ Business Logic $\to$ Data Access) emerged as the default enterprise pattern for organizing software. In Spring Boot, this pattern is formalized via stereotype annotations (`@RestController`, `@Service`, `@Repository`), enforcing unidirectional dependencies from top to bottom.

### Production Relevance
Layered architecture remains the most widely understood, quickly onboarded, and practical architectural pattern for greenfield web applications, CRUD services, internal administrative dashboards, and microservices with straightforward domain logic.

---

## 2. Core Concepts

### Mental Model & Architectural Layers

```
+-----------------------------------------------------------------------------------------+
|                                LAYERED ARCHITECTURE MODEL                               |
+-----------------------------------------------------------------------------------------+
|                                                                                         |
|  [ HTTP Client / Browser ]                                                             |
|           |                                                                             |
|           v                                                                             |
|  +-----------------------------------------------------------------------------------+  |
|  | 1. WEB / PRESENTATION LAYER (@RestController)                                     |  |
|  |    - Deserializes JSON to DTOs, validates input (@Valid)                          |  |
|  |    - Translates domain responses to HTTP status codes (200, 201, 404)             |  |
|  |    - Never executes business logic or direct database queries                     |  |
|  +-----------------------------------------------------------------------------------+  |
|           | (Calls DTO/Command objects)                                                 |
|           v                                                                             |
|  +-----------------------------------------------------------------------------------+  |
|  | 2. SERVICE / BUSINESS LAYER (@Service)                                            |  |
|  |    - Defines use cases, business rules, invariants, and workflows                 |  |
|  |    - Manages transaction boundaries (@Transactional)                              |  |
|  |    - Interacts with domain entities and orchestrates repositories                 |  |
|  +-----------------------------------------------------------------------------------+  |
|           | (Calls entities & query specs)                                              |
|           v                                                                             |
|  +-----------------------------------------------------------------------------------+  |
|  | 3. PERSISTENCE / DATA ACCESS LAYER (@Repository)                                  |  |
|  |    - Encapsulates database communication (Spring Data JPA, JDBC, jOOQ)             |  |
|  |    - Maps relational tables to Java Entities (@Entity)                            |  |
|  |    - Handles SQL queries, paging, and connection pooling                          |  |
|  +-----------------------------------------------------------------------------------+  |
|           |                                                                             |
|           v                                                                             |
|  [ Relational Database (PostgreSQL / MySQL) ]                                           |
+-----------------------------------------------------------------------------------------+
```

### Architectural Invariants
- **Strict Downward Dependency**: Layer $N$ may only call Layer $N-1$. Presentation calls Service; Service calls Repository. A Controller must *never* call a Repository directly.
- **No Upward Inversion**: Repositories must never be aware of Services; Services must never import Web/HTTP classes (`HttpServletRequest`, `ResponseEntity`).
- **Data Encapsulation**: DTOs cross between Client and Presentation; Domain models or Entities are processed in Service and Persistence.

---

## 3. How It Works Internally

### Request Execution Lifecycle Across Spring Beans

```
HTTP Request 
     |
     v
[ DispatcherServlet ]
     |
     v
[ OrderController ] -----------------------> Web Layer
     | (calls orderService.createOrder(dto))
     v
[ TransactionInterceptor (AOP Proxy) ] ----> Begins DB Transaction
     |
     v
[ OrderService ] --------------------------> Business Layer
     |
     +---> [ CustomerRepository ] ---------> Data Access Layer
     |        (fetches Customer entity)
     |
     +---> [ OrderRepository ] ------------> Data Access Layer
              (saves Order entity)
     |
[ TransactionInterceptor ] ----------------> Commits DB Transaction
     |
     v
[ OrderController ] -----------------------> Maps Entity to ResponseDTO
     |
     v
HTTP 201 Created (JSON Response)
```

1. **DispatcherServlet** maps the incoming URI to `OrderController.createOrder()`.
2. Spring's **Validator** executes Bean Validation (`@Valid`) on `CreateOrderRequest` before controller method execution.
3. Controller delegates to `OrderService` passing validated DTO parameters.
4. When entering `@Service` with `@Transactional`, Spring's AOP `TransactionInterceptor` intercepts the call, obtains a `Connection` from `HikariDataSource`, and disables autocommit (`connection.setAutoCommit(false)`).
5. The Service coordinates multiple `Repository` instances. Spring Data JPA delegates queries through `EntityManager` into Hibernate Session.
6. Upon successful completion of `OrderService`, the transaction commits and releases the JDBC connection back to HikariCP.
7. Controller transforms the returned entity into an immutable `OrderResponse` record and returns HTTP 201.

---

## 4. Syntax & Usage

### Stereotype Annotations and Clean Layer Boundaries

```java
package com.example.order.web;

import com.example.order.service.OrderService;
import com.example.order.web.dto.CreateOrderRequest;
import com.example.order.web.dto.OrderResponse;
import jakarta.validation.Valid;
import org.springframework.http.HttpStatus;
import org.springframework.web.bind.annotation.*;

@RestController
@RequestMapping("/api/v1/orders")
public class OrderController {

    private final OrderService orderService;

    public OrderController(OrderService orderService) {
        this.orderService = orderService;
    }

    @PostMapping
    @ResponseStatus(HttpStatus.CREATED)
    public OrderResponse createOrder(@Valid @RequestBody CreateOrderRequest request) {
        return orderService.createOrder(request);
    }
}
```

```java
package com.example.order.service;

import com.example.order.data.OrderEntity;
import com.example.order.data.OrderRepository;
import com.example.order.web.dto.CreateOrderRequest;
import com.example.order.web.dto.OrderResponse;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

@Service
@Transactional
public class OrderService {

    private final OrderRepository orderRepository;

    public OrderService(OrderRepository orderRepository) {
        this.orderRepository = orderRepository;
    }

    public OrderResponse createOrder(CreateOrderRequest request) {
        OrderEntity entity = new OrderEntity(request.customerId(), request.totalAmount());
        OrderEntity saved = orderRepository.save(entity);
        return new OrderResponse(saved.getId(), saved.getCustomerId(), saved.getTotalAmount());
    }
}
```

---

## 5. Practical Examples

### Example 1: Basic 3-Tier Layering with Java 21 Records

```java
package com.example.customer;

import jakarta.persistence.*;
import jakarta.validation.constraints.Email;
import jakarta.validation.constraints.NotBlank;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.http.ResponseEntity;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;
import org.springframework.web.bind.annotation.*;

import java.net.URI;
import java.util.UUID;

// 1. WEB DTOs
public record CreateCustomerRequest(@NotBlank String name, @Email String email) {}
public record CustomerResponse(UUID id, String name, String email) {}

// 2. DATA ENTITY
@Entity
@Table(name = "customers")
public class CustomerEntity {
    @Id @GeneratedValue(strategy = GenerationType.UUID)
    private UUID id;
    private String name;
    private String email;

    protected CustomerEntity() {}
    public CustomerEntity(String name, String email) {
        this.name = name;
        this.email = email;
    }
    public UUID getId() { return id; }
    public String getName() { return name; }
    public String getEmail() { return email; }
}

// 3. DATA REPOSITORY
public interface CustomerRepository extends JpaRepository<CustomerEntity, UUID> {
    boolean existsByEmail(String email);
}

// 4. BUSINESS SERVICE
@Service
@Transactional
public class CustomerService {
    private final CustomerRepository repository;

    public CustomerService(CustomerRepository repository) {
        this.repository = repository;
    }

    public CustomerResponse registerCustomer(CreateCustomerRequest req) {
        if (repository.existsByEmail(req.email())) {
            throw new IllegalArgumentException("Email already registered: " + req.email());
        }
        CustomerEntity saved = repository.save(new CustomerEntity(req.name(), req.email()));
        return new CustomerResponse(saved.getId(), saved.getName(), saved.getEmail());
    }
}

// 5. WEB CONTROLLER
@RestController
@RequestMapping("/api/customers")
public class CustomerController {
    private final CustomerService service;

    public CustomerController(CustomerService service) {
        this.service = service;
    }

    @PostMapping
    public ResponseEntity<CustomerResponse> register(@RequestBody CreateCustomerRequest req) {
        CustomerResponse res = service.registerCustomer(req);
        return ResponseEntity.created(URI.create("/api/customers/" + res.id())).body(res);
    }
}
```

### Example 2: Enterprise Layering with Domain Exceptions and ArchUnit Validation

Enforcing layer purity using **ArchUnit** tests:

```java
package com.example.architecture;

import com.tngtech.archunit.junit.AnalyzeClasses;
import com.tngtech.archunit.junit.ArchTest;
import com.tngtech.archunit.lang.ArchRule;

import static com.tngtech.archunit.library.Architectures.layeredArchitecture;

@AnalyzeClasses(packages = "com.example")
public class LayeredArchitectureTest {

    @ArchTest
    public static final ArchRule layer_dependencies_are_respected = layeredArchitecture()
        .consideringAllDependencies()
        .layer("Controller").definedBy("..web..")
        .layer("Service").definedBy("..service..")
        .layer("Repository").definedBy("..data..")

        .whereLayer("Controller").mayNotBeAccessedByAnyLayer()
        .whereLayer("Service").mayOnlyBeAccessedByLayers("Controller")
        .whereLayer("Repository").mayOnlyBeAccessedByLayers("Service");
}
```

### Example 3: Edge Case - Read-Only Query Layer Bypass (CQRS Optimization in Layered Architecture)

Sometimes, mapping entities through service abstractions for complex read queries adds latency without benefit:

```java
package com.example.reporting;

import org.springframework.jdbc.core.simple.JdbcClient;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.util.List;

public record OrderSummaryView(String customerName, long totalOrders, double totalSpent) {}

@Service
@Transactional(readOnly = true)
public class OrderReportingService {

    private final JdbcClient jdbcClient;

    public OrderReportingService(JdbcClient jdbcClient) {
        this.jdbcClient = jdbcClient;
    }

    public List<OrderSummaryView> getTopCustomerSummaries(int limit) {
        // High-performance read-only projection bypassing JPA entities entirely
        String sql = """
            SELECT c.name AS customerName, COUNT(o.id) AS totalOrders, SUM(o.total_amount) AS totalSpent
            FROM customers c
            JOIN orders o ON c.id = o.customer_id
            GROUP BY c.name
            ORDER BY totalSpent DESC
            LIMIT :limit
            """;

        return jdbcClient.sql(sql)
            .param("limit", limit)
            .query(OrderSummaryView.class)
            .list();
    }
}
```

### Example 4: Anti-Pattern (Leaking Repository & Entity to Web Layer) vs Fixed Architecture

#### The Anti-Pattern: Fat Controller Direct DB Access
```java
// BAD: Controller bypasses service, manipulates DB, and returns JPA Entity directly
@RestController
@RequestMapping("/bad/orders")
public class BadOrderController {

    private final OrderRepository repository;

    public BadOrderController(OrderRepository repository) {
        this.repository = repository;
    }

    @PostMapping("/{id}/cancel")
    public OrderEntity cancelOrder(@PathVariable Long id) {
        OrderEntity entity = repository.findById(id).orElseThrow();
        // Business logic inside web controller
        if ("SHIPPED".equals(entity.getStatus())) {
            throw new RuntimeException("Cannot cancel shipped order");
        }
        entity.setStatus("CANCELLED");
        return repository.save(entity); // Leaking JPA proxy entity with lazy relations
    }
}
```

#### Production Fix: Encapsulated Service and DTO Projection
```java
// GOOD: Proper 3-tier layering with business invariants in Service
@RestController
@RequestMapping("/orders")
public class CleanOrderController {

    private final OrderService service;

    public CleanOrderController(OrderService service) {
        this.service = service;
    }

    @PostMapping("/{id}/cancel")
    public ResponseEntity<OrderResponse> cancelOrder(@PathVariable Long id) {
        OrderResponse response = service.cancelOrder(id);
        return ResponseEntity.ok(response);
    }
}

@Service
@Transactional
public class CleanOrderService {

    private final OrderRepository repository;

    public CleanOrderService(OrderRepository repository) {
        this.repository = repository;
    }

    public OrderResponse cancelOrder(Long orderId) {
        OrderEntity order = repository.findById(orderId)
            .orElseThrow(() -> new OrderNotFoundException(orderId));

        // Domain rule verified in Service
        order.cancel(); 

        return OrderResponse.fromEntity(order);
    }
}
```

### Example 5: High-Performance Virtual-Threaded Asynchronous Layering

```java
package com.example.notification;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.scheduling.annotation.Async;
import org.springframework.stereotype.Service;

import java.time.Duration;

@Service
public class AsyncNotificationService {

    private static final Logger log = LoggerFactory.getLogger(AsyncNotificationService.class);

    // With Java 21 spring.threads.virtual.enabled=true, @Async runs on Virtual Threads
    @Async
    public void dispatchWelcomeNotification(String email, String name) {
        try {
            log.info("Sending notification to {} on thread: {}", email, Thread.currentThread());
            Thread.sleep(Duration.ofMillis(200)); // Non-blocking simulation on virtual thread
            log.info("Notification dispatched successfully to {}", email);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
    }
}
```

---

## 6. Best Practices

1. **Keep Controllers Thin**: Controllers should only handle HTTP routing, header extraction, validation, and status code mapping. Never put `if-else` business rules in controllers.
2. **Never Return JPA Entities from Controllers**: Returning JPA entities directly causes `LazyInitializationException`, cyclic serialization recursion (Jackson `StackOverflowError`), and leaks private database column structures.
3. **Always Isolate Transactions to the Service Layer**: Place `@Transactional` exclusively on `@Service` methods. Never annotate Controller endpoints or repository interfaces.
4. **Use Java Records for DTOs**: Use Java 21 `record` for request and response data carriers. They are immutable, concise, and provide built-in `equals()`, `hashCode()`, and `toString()`.
5. **Enforce Layer Rules with ArchUnit in CI/CD**: Add automated architecture tests that fail the build if a developer attempts to inject a `Repository` directly into a `Controller`.
6. **Package by Feature Rather Than Package by Layer**: Group classes into `com.example.order` (containing `OrderController`, `OrderService`, `OrderEntity`) rather than top-level `com.example.controllers` and `com.example.services` to improve cohesion.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| **Injecting Repository into Controller** | Bypasses business validation and transaction boundaries; controller becomes tightly coupled to SQL schema. | Always route requests through a `@Service` bean. |
| **Placing `@Transactional` on Controller** | Database connection is held across JSON serialization, prolonging connection checkout and starving HikariCP. | Move `@Transactional` exclusively to Service methods. |
| **Returning JPA Entities to API clients** | Jackson JSON serializer attempts to serialize lazy relationships, triggering `LazyInitializationException`. | Map JPA Entities to explicit immutable Record DTOs before returning. |
| **Anemic Domain Anti-Pattern** | Entity has only getters/setters; all logic is procedural code inside bloated 2,000-line service classes. | Encapsulate business invariants and state transitions inside the Entity methods (`order.cancel()`). |
| **Catching `Exception` in Service and swallowing it** | Prevents Spring's `TransactionInterceptor` from detecting failure; transaction commits partially corrupted state. | Allow unchecked exceptions to bubble up, or explicitly mark rollback: `@Transactional(rollbackFor = Exception.class)`. |
| **Top-Level Package-by-Layer sprawl** | Putting 100 controllers in one `controllers/` package destroys modularity and makes microservice extraction impossible. | Package by feature (`billing/`, `inventory/`, `customer/`). |

---

## 8. Performance & Security Notes

### Performance
- **Connection Checkout Window**: By keeping `@Transactional` confined to the Service layer, the database connection is acquired only during entity manipulation and released immediately before response DTO mapping and HTTP transport.
- **DTO Projections**: For read-heavy operations, bypass full entity hydration by using Spring Data Interface or Class DTO Projections to select only required SQL columns (`SELECT name, email FROM customer`).

### Security
- **Parameter Tampering Prevention**: Never bind HTTP request bodies directly to JPA entities. An attacker can send `{"id": 1, "role": "ADMIN"}` (mass assignment vulnerability). DTO records should strictly declare modifiable fields.
- **Input Sanitization**: Apply Jakarta Bean Validation (`@NotBlank`, `@Size`, `@Pattern`) on DTOs at the Controller boundary before business execution.

---

## 9. Comparison / When to Use What

| Architectural Dimension | Classic Layered Architecture | Hexagonal / Ports & Adapters | Clean / Onion Architecture |
|---|---|---|---|
| **Dependency Direction** | Top-down (Controller $\to$ Service $\to$ Repo) | Inward (Adapters $\to$ Ports $\to$ Domain Core) | Inward (Framework $\to$ UseCase $\to$ Domain) |
| **Coupling to Framework** | High (Spring annotations across all layers) | Low (Domain core has zero Spring dependencies) | Zero (Domain core contains pure Java POJOs) |
| **Complexity / Overhead** | Very Low (Minimal mapping boilerplate) | Moderate (Requires ports, adapters, mappers) | High (Strict interface contracts everywhere) |
| **Best Used For** | Standard CRUD, internal apps, simple domains | Complex enterprise domains, multi-channel I/O | Core enterprise systems with high longevity |
| **Learning Curve** | Instant for all Java developers | Requires DDD / inversion mindset | Steepest learning curve |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>1. Why is placing `@Transactional` on a `@RestController` considered a severe production hazard?</strong></summary>

When a Controller method is annotated with `@Transactional`:
1. Spring intercepts the incoming HTTP call and acquires a JDBC Connection from the pool *before* the controller executes.
2. The entire duration of controller execution—including any slow downstream REST calls, PDF generation, or third-party client calls—holds that physical DB connection open.
3. Even worse, the connection remains open during Jackson JSON serialization of the HTTP response body over slow client networks.
4. HikariCP connection pool gets depleted within seconds under moderate traffic, resulting in HTTP 504 timeouts across the entire microservice.
**Rule**: Always place `@Transactional` on `@Service` methods.
</details>

<details>
<summary><strong>2. What is the Anemic Domain Model and why does it frequently happen in Spring Boot layered architectures?</strong></summary>

The Anemic Domain Model (coined by Martin Fowler) occurs when domain entities are reduced to simple data bags containing only private fields, getters, and setters, while all business logic is placed into procedural `@Service` classes.
**Why it happens in Spring**: Developers get accustomed to auto-generating getters/setters with Lombok and treating entities as database schema mappings.
**Problems**:
- Invariants are violated because any caller can execute `entity.setStatus("APPROVED")` without checking preconditions.
- Services become massive, procedural "God classes" with duplicated validation logic.
**Solution**: Rich domain models. Entities should contain business methods (`order.approve(paymentConfirmation)`) and protect internal state.
</details>

<details>
<summary><strong>3. How does ArchUnit help maintain layered architecture integrity in continuous integration?</strong></summary>

In large engineering teams, developers inevitably introduce improper cross-layer references (e.g., calling a Repository directly from a Controller, or referencing a Controller class inside a Repository).
ArchUnit evaluates compiled Java bytecode against declarative architectural rules during unit tests. If a rule is violated (e.g., `whereLayer("Repository").mayOnlyBeAccessedByLayers("Service")`), the test suite fails immediately with an explanatory error and file line numbers, blocking the pull request before deployment.
</details>

<details>
<summary><strong>4. What is the difference between "Package-by-Layer" and "Package-by-Feature"? Which is preferred in modern Spring Boot?</strong></summary>

- **Package-by-Layer**: Code is organized by technical tier:
  `com.app.controller.*`, `com.app.service.*`, `com.app.repository.*`.
  *Problem*: Related files for a single business concept (e.g., `Order`) are scattered across the codebase. High coupling between packages; low cohesion.
- **Package-by-Feature**: Code is organized by business capability:
  `com.app.order.*` (containing Controller, Service, Repository, DTOs).
  *Advantage*: High cohesion. Developers work in a single directory. Package-private visibility (`default` scope) can be used to hide repository implementations from other modules. This is the recommended modern approach.
</details>

<details>
<summary><strong>5. Why should you never use JPA entities as request bodies (`@RequestBody`) or response bodies (`@ResponseBody`)?</strong></summary>

1. **Security Vulnerability (Mass Assignment)**: An attacker can send fields that should not be edited (e.g., `id`, `balance`, `isAdmin`), and Jackson will bind them directly onto the entity.
2. **Infinite Recursion**: Bidirectional JPA relationships (`@OneToMany` / `@ManyToOne`) cause Jackson to serialize back and forth infinitely, throwing `StackOverflowError`.
3. **LazyInitializationException**: Accessing lazily loaded collections during serialization outside a transaction throws exceptions.
4. **API Coupling**: Changing a database column name breaks the public API contract for external clients.
</details>

<details>
<summary><strong>6. Can a `@Service` bean call another `@Service` bean in a clean layered architecture?</strong></summary>

Yes, horizontal service-to-service communication is standard and acceptable in layered architecture, provided:
- **No Circular Dependencies**: `OrderService` calling `PaymentService` is fine, but `PaymentService` must not call `OrderService`.
- **Transaction Propagation**: The caller and callee must understand `@Transactional(propagation = Propagation.REQUIRED)` vs `REQUIRES_NEW` boundaries.
- Alternatively, for decoupled interactions, domain events (`ApplicationEventPublisher`) should be used.
</details>

<details>
<summary><strong>7. How does Spring Boot's Open Session in View (OSIV) anti-pattern affect layered architecture?</strong></summary>

By default, Spring Boot web starter enables `spring.jpa.open-in-view=true`.
OSIV binds a JPA `EntityManager` to the entire processing of an HTTP request, including the web view/controller layer.
**Architectural Violation**: It allows lazy associations to be loaded inside controllers or JSON serializers, masking architectural layer leaks.
**Performance Hazard**: Physical database connections are held open during network I/O, draining connection pools.
**Fix**: Always set `spring.jpa.open-in-view=false` in production.
</details>

<details>
<summary><strong>8. How do you handle cross-cutting concerns (logging, security, metrics) without cluttering layers?</strong></summary>

Cross-cutting concerns should be extracted using:
- **Spring Security Filters**: Authentication and token validation at the HTTP boundary.
- **Spring AOP / AspectJ**: Method execution timing, auditing, and metric recording.
- **HandlerInterceptor / ControllerAdvice**: Global exception handling and request tracing.
This keeps controllers, services, and repositories focused exclusively on their core responsibilities.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Refactor a Controller Leaking Entities to DTO Records</strong></summary>

**Objective**: Refactor the following endpoint so that no JPA entity is exposed in the API contract.

**Given**:
```java
@GetMapping("/{id}")
public UserEntity getUser(@PathVariable UUID id) {
    return userRepository.findById(id).orElseThrow();
}
```

**Solution**:
```java
public record UserResponse(UUID id, String username, String email) {
    public static UserResponse fromEntity(UserEntity entity) {
        return new UserResponse(entity.getId(), entity.getUsername(), entity.getEmail());
    }
}

@GetMapping("/{id}")
public UserResponse getUser(@PathVariable UUID id) {
    return userService.findById(id);
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Implement Service-Level Transactional Boundary</strong></summary>

**Objective**: Ensure that customer creation and initial credit allocation either both succeed or both roll back together.

**Solution**:
```java
@Service
@Transactional
public class CustomerRegistrationService {

    private final CustomerRepository customerRepo;
    private final WalletRepository walletRepo;

    public CustomerRegistrationService(CustomerRepository customerRepo, WalletRepository walletRepo) {
        this.customerRepo = customerRepo;
        this.walletRepo = walletRepo;
    }

    public void registerNewCustomer(String name, String email) {
        CustomerEntity customer = customerRepo.save(new CustomerEntity(name, email));
        walletRepo.save(new WalletEntity(customer.getId(), BigDecimal.valueOf(50.00))); // Bonus credit
    }
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Write an ArchUnit Rule Enforcing No Repository Access in Controllers</strong></summary>

**Objective**: Write a test verifying that no class in `*.web..` imports or accesses any class in `*.repository..`.

**Solution**:
```java
@AnalyzeClasses(packages = "com.example")
public class ArchitectureRulesTest {

    @ArchTest
    public static final ArchRule controllers_must_not_access_repositories = 
        noClasses().that().resideInAPackage("..web..")
            .should().accessClassesThat().resideInAPackage("..repository..");
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Implement Package-Private Visibility for Encapsulated Repositories</strong></summary>

**Objective**: Use Java package-private access modifier to hide `ProductRepository` from any class outside `com.example.product`.

**Solution**:
```java
package com.example.product;

// Package-private interface: cannot be injected or referenced outside com.example.product
interface ProductRepository extends JpaRepository<ProductEntity, Long> {
}

// Public service: only allowed entry point for other modules
@Service
public class ProductService {
    private final ProductRepository repository;

    ProductService(ProductRepository repository) {
        this.repository = repository;
    }

    public ProductResponse getProduct(Long id) {
        return repository.findById(id).map(ProductResponse::from).orElseThrow();
    }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Implement Global Layered Exception Mapping with ProblemDetail (RFC 7807)</strong></summary>

**Objective**: Map domain-layer exceptions (`EntityNotFoundException`, `BusinessRuleException`) to RFC 7807 JSON errors in the presentation layer.

**Solution**:
```java
@RestControllerAdvice
public class GlobalExceptionHandler extends ResponseEntityExceptionHandler {

    @ExceptionHandler(EntityNotFoundException.class)
    public ProblemDetail handleNotFound(EntityNotFoundException ex) {
        ProblemDetail problem = ProblemDetail.forStatusAndDetail(HttpStatus.NOT_FOUND, ex.getMessage());
        problem.setTitle("Resource Not Found");
        problem.setType(URI.create("https://api.example.com/errors/not-found"));
        return problem;
    }

    @ExceptionHandler(BusinessRuleViolationException.class)
    public ProblemDetail handleBusinessRule(BusinessRuleViolationException ex) {
        ProblemDetail problem = ProblemDetail.forStatusAndDetail(HttpStatus.UNPROCESSABLE_ENTITY, ex.getMessage());
        problem.setTitle("Business Rule Violation");
        problem.setProperty("ruleCode", ex.getErrorCode());
        return problem;
    }
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Build Read/Write Split CQRS Repositories in a Layered Service</strong></summary>

**Objective**: Configure a service that uses Spring Data JPA for state changes (Write layer) and `JdbcClient` with read-only connection pooling for queries (Read layer).

**Solution**:
```java
@Service
public class OptimizedOrderService {

    private final OrderJpaRepository writeRepo;
    private final JdbcClient readClient;

    public OptimizedOrderService(OrderJpaRepository writeRepo, JdbcClient readClient) {
        this.writeRepo = writeRepo;
        this.readClient = readClient;
    }

    @Transactional
    public Long createOrder(CreateOrderRequest req) {
        OrderEntity entity = new OrderEntity(req.customerId(), req.amount());
        return writeRepo.save(entity).getId();
    }

    @Transactional(readOnly = true)
    public List<OrderCardDto> fetchOrderCards(UUID customerId) {
        return readClient.sql("SELECT id, amount, status FROM orders WHERE customer_id = :id")
            .param("id", customerId)
            .query(OrderCardDto.class)
            .list();
    }
}
```
</details>

---

## 12. Summary & Cheat Sheet

```
+----------------------------------------------------------------------------------------------------+
| LAYERED ARCHITECTURE QUICK REFERENCE                                                               |
+----------------------------------------------------------------------------------------------------+
| Layer               | Primary Annotations         | Responsibilities           | Forbidden Items   |
+---------------------+-----------------------------+----------------------------+-------------------+
| Presentation (Web)  | @RestController             | HTTP routing, JSON DTOs,   | Business rules,   |
|                     | @RequestMapping, @Valid     | validation, status codes   | direct SQL/Repo   |
+---------------------+-----------------------------+----------------------------+-------------------+
| Business (Service)  | @Service, @Transactional    | Domain logic, transaction  | HttpServletReq,   |
|                     | @Validated                  | boundary, orchestration    | UI formatting     |
+---------------------+-----------------------------+----------------------------+-------------------+
| Persistence (Data)  | @Repository, @Entity        | Database queries, mapping, | HTTP DTOs,        |
|                     | JpaRepository, JdbcClient   | relational schema binding  | business rules    |
+---------------------+-----------------------------+----------------------------+-------------------+
```

---

## 13. Further Reading
- [Patterns of Enterprise Application Architecture (Martin Fowler)](https://martinfowler.com/books/eaa.html)
- [ArchUnit User Guide: Architecture Testing in Java](https://www.archunit.org/user-guide/html/000_Index.html)
- [Spring Data JPA Reference Documentation](https://docs.spring.io/spring-data/jpa/reference/)
- [Domain-Driven Design: Tackling Complexity in the Heart of Software (Eric Evans)](https://www.domainlanguage.com/ddd/)
- [RFC 7807: Problem Details for HTTP APIs](https://datatracker.ietf.org/doc/html/rfc7807)
