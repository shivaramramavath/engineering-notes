# Project Structure, Packaging & Enterprise Conventions

> Master industry-standard project topologies for enterprise Spring Boot backends: Package-by-Layer vs Package-by-Feature (Vertical Slices), Hexagonal / Clean Architecture (Ports & Adapters), Spring Modulith boundaries, and automated architectural rule enforcement with ArchUnit.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [Spring Ecosystem Overview](file:///03-backend/spring-boot/00-setup/01-spring-ecosystem-overview.md), [First Application Walkthrough](file:///03-backend/spring-boot/00-setup/05-first-application-walkthrough.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.0.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 25.0.1, Spring Boot 3.4.0 |

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
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### Problem Statement
Without rigorous package organization and architectural constraints, enterprise Java codebases rapidly degenerate into "big balls of mud." In such systems, web controllers directly invoke JPA repositories, domain entities leak across HTTP boundaries, cyclical dependencies prevent testing, and changes to one feature inadvertently break unrelated business modules.

### Historical Evolution
- **Traditional Layered Packaging (2000-2012)**: Code was partitioned horizontally into `controllers`, `services`, and `daos`. All entities resided in one folder, making every class `public` and destroying modular encapsulation.
- **Package-by-Feature & Vertical Slices (2012-2020)**: Developers realized that organizing code by business capability (`order`, `customer`, `billing`) co-located related concepts, allowed package-private visibility, and simplified refactoring.
- **Hexagonal Architecture & Spring Modulith (2020-Present)**: Modern microservices and modular monoliths enforce strict dependency inversion: domain cores remain agnostic of frameworks, and tools like ArchUnit and Spring Modulith statically verify architectural integrity on every build.

### Real-World Relevance
Selecting the right structural layout and enforcing it with automated architecture tests ensures codebases remain maintainable, testable, and cleanly decomposable into independent microservices when scale demands it.

---

## 2. Core Concepts

### 1. Package-by-Layer vs. Package-by-Feature
- **Package-by-Layer (Horizontal)**: Groups classes by their technical role (`com.example.controller`, `com.example.service`, `com.example.repository`).
  * *Disadvantage*: Requires almost all classes to be `public`, obscuring domain cohesion.
- **Package-by-Feature (Vertical Slice)**: Groups classes by domain capability (`com.example.order`, `com.example.payment`, `com.example.customer`).
  * *Advantage*: Internal helper classes, queries, and state machines can remain package-private (`default` visibility), exposing only a clean public interface or record.

### 2. Hexagonal Architecture (Ports and Adapters)
Isolates the domain business logic from technical infrastructure (HTTP, SQL, Kafka):
- **Core (Domain & Application)**: Contains domain models, value objects, business rules, and inbound/outbound ports (Java interfaces).
- **Adapters (Inbound & Outbound)**: Implements ports. Inbound adapters translate REST requests into domain commands; outbound adapters implement database persistence or messaging.

### 3. Java Visibility as an Architectural Tool
Java offers four visibility levels: `public`, `protected`, `package-private` (no modifier), and `private`. Enterprise architectures leverage package-private visibility to hide implementation details from sibling packages without needing separate Maven modules.

### 4. Automated Verification with ArchUnit
ArchUnit is a testing library for verifying architecture rules (package dependencies, naming conventions, circular references, layer isolation) as standard JUnit 5 tests.

---

## 3. How It Works Internally

### Enterprise Architectural Topologies

```
A. PACKAGE-BY-FEATURE / VERTICAL SLICES
+-------------------------------------------------------------------------+
| com.example.orders (Root Module)                                        |
|  +-- OrderController.java (Public: REST Ingress)                        |
|  +-- OrderService.java (Public interface or package-private service)   |
|  +-- OrderRepository.java (Package-private Spring Data interface)      |
|  +-- OrderEntity.java (Package-private JPA entity)                      |
|  +-- OrderCreatedEvent.java (Public event record)                       |
+-------------------------------------------------------------------------+

B. HEXAGONAL ARCHITECTURE (PORTS & ADAPTERS)
                      +-----------------------------+
                      |   REST Controller (Adapter) |
                      +--------------+--------------+
                                     | Calls Inbound Port
                                     v
                      +-----------------------------+
                      |       Inbound Port          |
                      | (OrderUseCase.java Interf.) |
                      +--------------+--------------+
                                     |
                      +--------------v--------------+
                      |      DOMAIN & BUSINESS      |
                      |   (OrderAggregate, Rules)   |
                      +--------------+--------------+
                                     |
                      +--------------v--------------+
                      |       Outbound Port         |
                      | (OrderPort.java Interface)  |
                      +--------------+--------------+
                                     ^ Implements Outbound Port
                                     |
                      +--------------+--------------+
                      |  PostgreSQL / JPA (Adapter) |
                      +-----------------------------+
```

---

## 4. Syntax & Usage

### Recommended Project Directory Layout (Maven)
```text
my-service/
├── .mvn/
│   └── wrapper/
│       └── maven-wrapper.properties
├── mvnw
├── mvnw.cmd
├── pom.xml
└── src/
    ├── main/
    │   ├── java/
    │   │   └── com/
    │   │       └── example/
    │   │           └── myservice/
    │   │               ├── MyServiceApplication.java
    │   │               ├── common/
    │   │               │   ├── error/
    │   │               │   └── security/
    │   │               ├── customer/
    │   │               │   ├── CustomerController.java
    │   │               │   ├── CustomerService.java
    │   │               │   ├── CustomerRepository.java
    │   │               │   └── Customer.java
    │   │               └── order/
    │   │                   ├── OrderController.java
    │   │                   ├── OrderService.java
    │   │                   ├── OrderRepository.java
    │   │                   └── Order.java
    │   └── resources/
    │       ├── application.yaml
    │       ├── application-dev.yaml
    │       ├── db/
    │       │   └── migration/
    │       │       └── V1__initial_schema.sql
    │       └── logback-spring.xml
    └── test/
        ├── java/
        │   └── com/
        │       └── example/
        │           └── myservice/
        │               ├── architecture/
        │               │   └── ArchitectureTest.java
        │               └── order/
        │                   └── OrderControllerTest.java
        └── resources/
            └── application-test.yaml
```

---

## 5. Practical Examples

### Example 1: Package-by-Feature with Package-Private Encapsulation
```java
// Package: com.example.myservice.order
package com.example.myservice.order;

import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;
import java.util.UUID;

// PUBLIC: Exposed API entry point
@RestController
@RequestMapping("/api/v1/orders")
public class OrderController {

    private final OrderService orderService;

    public OrderController(OrderService orderService) {
        this.orderService = orderService;
    }

    @PostMapping
    public ResponseEntity<OrderResponse> createOrder(@RequestBody CreateOrderRequest request) {
        OrderResponse response = orderService.placeOrder(request);
        return ResponseEntity.ok(response);
    }
}

// PUBLIC: Shared Request/Response DTO records
public record CreateOrderRequest(String customerId, double totalAmount) {}
public record OrderResponse(UUID orderId, String status) {}

// PACKAGE-PRIVATE: Implementation service hidden from sibling packages
@org.springframework.stereotype.Service
class OrderService {

    private final OrderRepository repository;

    OrderService(OrderRepository repository) {
        this.repository = repository;
    }

    OrderResponse placeOrder(CreateOrderRequest req) {
        OrderEntity entity = new OrderEntity(UUID.randomUUID(), req.customerId(), req.totalAmount());
        repository.save(entity);
        return new OrderResponse(entity.getId(), "CONFIRMED");
    }
}

// PACKAGE-PRIVATE: Internal JPA Entity and Repository
@jakarta.persistence.Entity
@jakarta.persistence.Table(name = "orders")
class OrderEntity {
    @jakarta.persistence.Id
    private UUID id;
    private String customerId;
    private double amount;

    protected OrderEntity() {}
    OrderEntity(UUID id, String customerId, double amount) {
        this.id = id;
        this.customerId = customerId;
        this.amount = amount;
    }
    public UUID getId() { return id; }
}

interface OrderRepository extends org.springframework.data.jpa.repository.JpaRepository<OrderEntity, UUID> {}
```

### Example 2: Hexagonal Architecture Port & Adapter
```java
// 1. Inbound Port: com.example.ports.inbound.PlaceOrderUseCase.java
package com.example.ports.inbound;

import java.util.UUID;

public interface PlaceOrderUseCase {
    UUID execute(PlaceOrderCommand command);
    record PlaceOrderCommand(String customerId, double amount) {}
}

// 2. Outbound Port: com.example.ports.outbound.OrderStoragePort.java
package com.example.ports.outbound;

import java.util.UUID;

public interface OrderStoragePort {
    void persistOrder(UUID id, String customerId, double amount);
}

// 3. Domain Service (Zero Spring imports): com.example.domain.OrderDomainService.java
package com.example.domain;

import com.example.ports.inbound.PlaceOrderUseCase;
import com.example.ports.outbound.OrderStoragePort;
import java.util.UUID;

public class OrderDomainService implements PlaceOrderUseCase {

    private final OrderStoragePort storagePort;

    public OrderDomainService(OrderStoragePort storagePort) {
        this.storagePort = storagePort;
    }

    @Override
    public UUID execute(PlaceOrderCommand cmd) {
        if (cmd.amount() <= 0) {
            throw new IllegalArgumentException("Order amount must be positive");
        }
        UUID orderId = UUID.randomUUID();
        storagePort.persistOrder(orderId, cmd.customerId(), cmd.amount());
        return orderId;
    }
}
```

### Example 3: Automated Architectural Guardrails with ArchUnit
```java
package com.example.myservice.architecture;

import com.tngtech.archunit.core.importer.ImportOption;
import com.tngtech.archunit.junit.AnalyzeClasses;
import com.tngtech.archunit.junit.ArchTest;
import com.tngtech.archunit.lang.ArchRule;
import org.springframework.stereotype.Controller;
import org.springframework.web.bind.annotation.RestController;

import static com.tngtech.archunit.lang.syntax.ArchRuleDefinition.*;
import static com.tngtech.archunit.library.Architectures.layeredArchitecture;

@AnalyzeClasses(packages = "com.example.myservice", importOptions = ImportOption.DoNotIncludeTests.class)
public class ArchitectureTest {

    // Rule 1: Controllers must not access Repositories directly
    @ArchTest
    static final ArchRule controllers_must_not_access_repositories =
        noClasses().that().areAnnotatedWith(RestController.class)
            .or().areAnnotatedWith(Controller.class)
            .should().dependOnClassesThat().haveSimpleNameEndingWith("Repository");

    // Rule 2: Service classes must reside in domain or service packages
    @ArchTest
    static final ArchRule services_must_reside_in_service_packages =
        classes().that().haveSimpleNameEndingWith("Service")
            .should().resideInAnyPackage("..service..", "..order..", "..customer..");

    // Rule 3: No field injection allowed (forces constructor DI)
    @ArchTest
    static final ArchRule no_field_injection =
        noFields().should().beAnnotatedWith("org.springframework.beans.factory.annotation.Autowired")
            .because("Constructor injection is mandatory across all enterprise services");
}
```

### Example 4: Spring Modulith Modular Verification
```java
package com.example.myservice;

import org.junit.jupiter.api.Test;
import org.springframework.modulith.core.ApplicationModules;
import org.springframework.modulith.docs.Documenter;

class ModulithArchitectureTest {

    private final ApplicationModules modules = ApplicationModules.of(MyServiceApplication.class);

    @Test
    void verifyModularStructure() {
        // Statically analyzes package dependencies and asserts zero illegal cross-module couplings
        modules.verify();
    }

    @Test
    void generateDocumentation() {
        // Generates PlantUML component and C4 diagrams into target/spring-modulith-docs
        new Documenter(modules).writeDocumentation();
    }
}
```

### Example 5: Architectural Failure Anti-Pattern & Remediation
```java
// ANTI-PATTERN: Leaking JPA entities directly to REST Clients
@RestController
public class BadOrderController {
    @Autowired
    private OrderRepository repository; // BAD: Controller talks to DB directly

    @GetMapping("/{id}")
    public OrderEntity getOrder(@PathVariable UUID id) {
        return repository.findById(id).orElseThrow(); // BAD: Entity exposed over HTTP
    }
}

// REMEDIATION: Layered separation with DTO Records
@RestController
@RequestMapping("/api/v1/orders")
public class CleanOrderController {
    private final OrderService service; // GOOD: Injected via constructor

    public CleanOrderController(OrderService service) {
        this.service = service;
    }

    @GetMapping("/{id}")
    public ResponseEntity<OrderDto> getOrder(@PathVariable UUID id) {
        OrderDto dto = service.findOrder(id); // GOOD: Returns decoupled DTO
        return ResponseEntity.ok(dto);
    }
}
```

---

## 6. Best Practices

1. **Default to Package-Private Visibility**: Mark services, repositories, and entities package-private whenever possible. Expose only REST controllers and public DTOs.
2. **Never Expose JPA Entities Over HTTP**: Exposing entities leads to unintended database modifications, serialization recursion (`StackOverflowError`), and breaks API backward compatibility.
3. **Enforce Clean Boundaries with ArchUnit**: Run ArchUnit tests in CI on every commit to catch architectural violations before code reviews.
4. **Co-locate Tests with Features**: Mirror feature package layouts in `src/test/java` for intuitive navigation.
5. **Separate Database Migrations by Version**: Store Flyway migration scripts in `src/main/resources/db/migration` using strict semantic naming: `V1__init.sql`, `V2__add_index.sql`.
6. **Use Spring Modulith for Monoliths**: Verify module interaction rules and asynchronous event boundaries with `ApplicationModules.of(App.class).verify()`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Making every class `public`** | Breaks encapsulation; any class in the codebase can instantiate or bypass domain rules. | Strip `public` modifiers from internal services, entities, and repositories. |
| **Direct Controller-to-Repository calls** | Bypasses business validation, transaction boundaries, and auditing interceptors. | Enforce service layer indirection via ArchUnit rules. |
| **Cyclic package dependencies** | Package A depends on Package B, which depends on Package A. Causes testing nightmares and prevents modularization. | Decouple using domain events (`ApplicationEventPublisher`) or port interfaces. |
| **Misplaced Main Application Class** | Putting the main class in a sub-package causes `@ComponentScan` to miss sibling packages. | Ensure the main class is in the root parent package. |
| **Inconsistent Configuration Key Naming** | Using camelCase in some YAML keys and kebab-case in others causes configuration binding confusion. | Standardize on **kebab-case** (`order-service.retry-count: 3`). |
| **Putting Integration Test Configs in Main Class** | Hardcoding test mocks in main configurations causes production bugs. | Use dedicated `@TestConfiguration` classes in `src/test/java`. |

---

## 8. Performance & Security Notes

### Encapsulation Reduces Attack Surface
Restricting classes to package-private visibility prevents malicious or unintended reflection and injection by third-party libraries or rogue beans.

### Compile-Time Boundary Enforcement
Multi-module builds or ArchUnit tests enforce architectural boundaries at compile/test time, eliminating runtime reflection overhead and preventing memory leaks caused by circular proxy chains.

---

## 9. Comparison / When to Use What

| Architecture Style | Complexity | Modularity | Team Scalability | Best For |
| :--- | :--- | :--- | :--- | :--- |
| **Package-by-Layer** | Low | Low (Leaky boundaries) | 1-3 engineers | Simple CRUD apps, prototypes |
| **Package-by-Feature (Vertical)** | Low-Medium | High (Encapsulated) | 3-15 engineers | Enterprise microservices, REST APIs |
| **Hexagonal / Clean** | High | Very High (Framework-free core) | Large teams | Core banking, complex domain logic |
| **Spring Modulith** | Medium | High (Formally verified) | Large teams | Large monoliths avoiding microservice sprawl |

---

## 10. Interview Questions

<details>
<summary><strong>Q1: What are the main limitations of the traditional "Package-by-Layer" project structure?</strong></summary>

**Answer:**  
In package-by-layer (`controllers`, `services`, `repositories`), every class that needs to be accessed by a neighboring layer must be declared `public`. This completely bypasses Java's package-private encapsulation, meaning any controller can directly call any repository across the entire system. Furthermore, understanding a single business capability requires jumping across multiple directories.
</details>

<details>
<summary><strong>Q2: Why should JPA entities never be returned directly from REST controllers?</strong></summary>

**Answer:**  
1. **Security / Information Disclosure**: Entities often contain sensitive fields (passwords, audit metadata, internal IDs) that leak over the wire if serialized.
2. **LazyInitializationException**: Jackson attempting to serialize uninitialized lazy relationships outside an active transaction will fail.
3. **Serialization Loops**: Bidirectional relationships (`@OneToMany` / `@ManyToOne`) cause infinite loops and `StackOverflowError`.
4. **Coupling**: Any database schema modification inadvertently breaks external API contracts.
</details>

<details>
<summary><strong>Q3: How does ArchUnit help maintain architecture in enterprise teams?</strong></summary>

**Answer:**  
ArchUnit parses Java bytecode and compiles it into an in-memory graph. It allows developers to express architectural constraints (e.g., "classes in controller package must never be accessed by classes in repository package") as executable JUnit 5 assertions. If an engineer introduces an illegal coupling, the build immediately fails in CI.
</details>

<details>
<summary><strong>Q4: What is the benefit of Spring Modulith over microservices?</strong></summary>

**Answer:**  
Spring Modulith provides the architectural boundaries, event-driven decoupling, and domain isolation of microservices within a single, easily deployable monolith. It eliminates distributed tracing complexity, network latency, and infrastructure maintenance costs while offering automated validation that modules obey strict dependency rules.
</details>

<details>
<summary><strong>Q5: What is the purpose of the `common` or `shared` package, and why does it often become problematic?</strong></summary>

**Answer:**  
A `common` package is intended for cross-cutting utilities (e.g., date helpers, global exception handling, custom validation annotations). However, without discipline, it often turns into an unstructured dumping ground for domain models, causing tight coupling across otherwise independent modules.
</details>

<details>
<summary><strong>Q6: In Hexagonal Architecture, what is the difference between an Inbound Port and an Outbound Port?</strong></summary>

**Answer:**  
- **Inbound Port (Driving)**: An interface exposed by the domain application layer that defines use cases that external actors (REST controllers, CLI commands, Kafka listeners) can call.
- **Outbound Port (Driven)**: An interface defined by the domain layer that specifies infrastructure needs (e.g., saving data, sending an SMS) that external adapters (JPA repository adapter, Twilio adapter) must implement.
</details>

<details>
<summary><strong>Q7: What is the naming convention for Spring Boot configuration property keys?</strong></summary>

**Answer:**  
Spring Boot mandates **kebab-case** for property names in configuration files (e.g., `spring.datasource.max-active: 20`). Spring Boot's Relaxed Binding mechanism automatically maps kebab-case keys to camelCase Java record or POJO properties (`maxActive`).
</details>

<details>
<summary><strong>Q8: How can domain events be used to decouple two packages within the same Spring Boot application?</strong></summary>

**Answer:**  
Instead of the `OrderService` directly calling `NotificationService` or `InventoryService`, the `OrderService` publishes an immutable event record using `ApplicationEventPublisher.publishEvent(new OrderPlacedEvent(...))`. The `NotificationService` listens via `@ApplicationModuleListener` or `@EventListener`, decoupling the packages completely.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Convert Public Classes to Package-Private (Easy)</strong></summary>

**Task:**  
In a vertical slice feature package (`com.example.billing`), remove the `public` modifier from the `BillingRepository` interface and `BillingService` implementation class. Verify that the `BillingController` can still access them, but classes in another package cannot.
</details>

<details>
<summary><strong>Exercise 2: Write an ArchUnit Naming Convention Rule (Easy)</strong></summary>

**Task:**  
Create an ArchUnit test asserting that all classes annotated with `@RestController` must have simple class names ending with `Controller` and reside in a package matching `..controller..` or `..api..`.
</details>

<details>
<summary><strong>Exercise 3: Write an ArchUnit Layer Isolation Rule (Medium)</strong></summary>

**Task:**  
Write an ArchUnit test that prevents any class in `com.example.service` from importing classes from `com.example.controller`, ensuring that dependencies strictly flow inward.
</details>

<details>
<summary><strong>Exercise 4: Verify Spring Modulith Structure (Medium)</strong></summary>

**Task:**  
Add `spring-modulith-starter-test` to `pom.xml`. Create a test running `ApplicationModules.of(App.class).verify()`. Introduce an illegal package reference from module `order` into an internal package of module `customer`, and observe the build failure and diagnostic output.
</details>

<details>
<summary><strong>Exercise 5: Hexagonal Architecture Port Implementation (Hard)</strong></summary>

**Task:**  
Refactor a tightly coupled controller-service-repository feature into Hexagonal Architecture:
1. Create an inbound use case interface `CreateAccountUseCase`.
2. Create an outbound port `AccountRepositoryPort`.
3. Create a domain service with no framework imports implementing the use case.
4. Implement the outbound port using a Spring Data JPA adapter.
</details>

<details>
<summary><strong>Exercise 6: Enforce Zero Field Injection Across Whole Repository (Hard)</strong></summary>

**Task:**  
Write an ArchUnit test rule that scans all project packages and asserts that no field contains `@Autowired` or `@Inject`. Ensure that any violation outputs a clear remediation message explaining the benefits of constructor injection.
</details>

---

## 12. Summary / Cheat Sheet

| Architectural Concept | Convention / Rule | Benefit |
| :--- | :--- | :--- |
| **Package-by-Feature** | `com.example.<feature>` | Co-locates related files, enables package-private scoping |
| **Encapsulation** | Omit `public` on services and repositories | Enforces interaction through public controllers or ports |
| **DTO Separation** | Never return JPA entities over HTTP | Prevents leaks, serialization cycles, and breaking changes |
| **ArchUnit Test** | `noClasses().that().areAnnotatedWith(RestController.class)...` | Automated enforcement of architectural integrity in CI |
| **Spring Modulith** | `ApplicationModules.of(App.class).verify()` | Guarantees modular monolith integrity and documentation |
| **Constructor DI** | Explicit constructor with `final` fields | Immutable state, easy testing, null safety |
| **Property Syntax** | Kebab-case (`server.shutdown: graceful`) | Standardized Spring Boot relaxed binding convention |

---

## 13. Further Reading
- [Spring Modulith Reference Documentation](https://docs.spring.io/spring-modulith/reference/)
- [ArchUnit User Guide](https://www.archunit.org/user-guide/html/000_Index.html)
- [Hexagonal Architecture by Alistair Cockburn](https://alistair.cockburn.us/hexagonal-architecture/)
- [Clean Architecture by Robert C. Martin](https://blog.cleancoder.com/uncle-bob/2012/08/13/the-clean-architecture.html)
