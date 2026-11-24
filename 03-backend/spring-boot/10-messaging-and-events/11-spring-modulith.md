# Spring Modulith: Modular Monoliths, Architectural Verification, and Event Publication

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | `org.springframework.modulith:spring-modulith-starter-core:1.3.x`, `spring-modulith-starter-jdbc` |

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
- [8. Performance & Security Notes](#8-performance--security-notes)
- [9. Comparison / When to Use What](#9-comparison--when-to-use-what)
- [10. Interview Questions](#10-interview-questions)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

### The Problem
For many organizations, jumping prematurely into microservices creates massive operational complexity: distributed transactions, network latency, distributed logging headaches, container orchestration overhead, and high cloud hosting costs. Conversely, traditional monolithic applications inevitably degrade over time into an unmaintainable "Big Ball of Mud": boundaries between domains dissolve, internal services reach directly into other packages' internal classes, and circular dependencies make refactoring impossible.

### The Evolution
**Spring Modulith** is an official Spring project designed to support well-structured, domain-aligned **Modular Monoliths**. It provides first-class architectural governance for Spring Boot applications. Developers organize code into clean, logical domain modules within a single repository and runtime. Spring Modulith automatically verifies package access rules during unit tests, prevents illegal cross-module dependencies, generates living architectural diagrams (C4 models and PlantUML), and provides a zero-configuration, transactional, persistent **Event Publication Registry** (Outbox pattern) for asynchronous inter-module events.

### Relevance in Modern Engineering
Spring Modulith offers the optimal balance: the operational simplicity of a single deployable artifact (one JVM, one database) paired with the strict boundary isolation, testability, and event-driven decoupling of microservices. If an organization ever needs to extract a module into a standalone microservice, the module's strict boundaries make the extraction trivial.

---

## 2. Core Concepts

- **Application Module**: A logical unit of domain functionality represented by a first-level sub-package under the main application root package (e.g., `com.example.app.order`, `com.example.app.inventory`).
- **Public API vs Internal Implementation**:
  - *Public API*: Classes located directly in the root of the module package (accessible to other modules).
  - *Internal Implementation*: Classes in sub-packages (e.g., `com.example.app.order.internal`) are completely hidden from other modules.
- **Architectural Verification**: Static analysis using ArchUnit behind the scenes (`ApplicationModules.of(Application.class).verify()`) verifying that no module violates package boundaries or cyclic rules.
- **Event Publication Registry**: A persistent transaction-aware event outbox (`spring-modulith-starter-jdbc`) that records published domain events in an event publication log table, guaranteeing at-least-once delivery to `@ApplicationModuleListener` components.
- **`@ApplicationModuleListener`**: Specialized Spring Modulith annotation combining `@Async`, `@TransactionalEventListener(phase = AFTER_COMMIT)`, and transaction boundary isolation.
- **Living Documentation**: Programmatic generation of PlantUML component diagrams, C4 models, and AsciiDoc architectural canvases from the codebase during builds.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        Spring Modulith Module Architecture                        |
|                                                                                   |
| Root Package: com.example.platform                                                |
|                                                                                   |
|  +--------------------------------+       +------------------------------------+  |
|  | Module: Order                  |       | Module: Inventory                  |  |
|  | (com.example.platform.order)   |       | (com.example.platform.inventory)   |  |
|  |                                |       |                                    |  |
|  | [Public API]                   |       | [Public API]                       |  |
|  | OrderService.java              |       | InventoryService.java              |  |
|  | OrderCreatedEvent.java (record)|       |                                    |  |
|  |                                |       | [Internal - Package Private]       |  |
|  | [Internal - Hidden]            |       | StockEntity.java                   |  |
|  | internal.OrderRepository.java  |       | internal.StockRepository.java      |  |
|  +--------------------------------+       +------------------------------------+  |
|                 |                                             ^                   |
|                 | Publishes OrderCreatedEvent                 | Listens to event  |
|                 v                                             |                   |
|  +-----------------------------------------------------------------------------+  |
|  |                     Event Publication Registry (Outbox)                     |  |
|  |  Table: EVENT_PUBLICATION (Persisted atomically in current local DB tx)     |  |
|  |  Dispatches asynchronously to @ApplicationModuleListener in Inventory      |  |
|  |  Marks publication completed once Inventory module listener succeeds        |  |
|  +-----------------------------------------------------------------------------+  |
+-----------------------------------------------------------------------------------+
```

During build or unit test execution, `ApplicationModules.of(App.class).verify()` inspects the classloader via ArchUnit. It ensures that classes in `inventory` never import classes from `order.internal` and that no circular dependencies exist between modules. At runtime, when `OrderService` publishes `OrderCreatedEvent`, Spring Modulith intercepts the event, writes it to the `EVENT_PUBLICATION` table in the same database transaction, and delivers it to `@ApplicationModuleListener` methods. Once the listener completes without errors, the publication entry is marked completed.

---

## 4. Syntax & Usage

### Maven Dependency
```xml
<dependency>
    <groupId>org.springframework.modulith</groupId>
    <artifactId>spring-modulith-starter-core</artifactId>
    <version>1.3.0</version>
</dependency>
<dependency>
    <groupId>org.springframework.modulith</groupId>
    <artifactId>spring-modulith-starter-jdbc</artifactId>
    <version>1.3.0</version>
</dependency>
<dependency>
    <groupId>org.springframework.modulith</groupId>
    <artifactId>spring-modulith-starter-test</artifactId>
    <version>1.3.0</version>
    <scope>test</scope>
</dependency>
```

### Application Properties
```properties
# Event Publication Registry Database Configuration
spring.modulith.republish-outstanding-events-on-restart=true
```

---

## 5. Practical Examples

### Example 1: Defining a Public Domain Event in Module `order`
```java
package com.example.platform.order;

import java.time.Instant;
import java.util.UUID;

// Publicly visible domain event: placed in root of 'order' package
public record OrderPlacedEvent(
        UUID orderId,
        String customerId,
        double totalAmount,
        Instant placedAt
) {}
```

### Example 2: Order Service Publishing Event Within Local Transaction
```java
package com.example.platform.order;

import com.example.platform.order.internal.OrderEntity;
import com.example.platform.order.internal.OrderRepository;
import org.springframework.context.ApplicationEventPublisher;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.time.Instant;
import java.util.UUID;

@Service
public class OrderService {

    private final OrderRepository orderRepository;
    private final ApplicationEventPublisher eventPublisher;

    public OrderService(OrderRepository orderRepository, ApplicationEventPublisher eventPublisher) {
        this.orderRepository = orderRepository;
        this.eventPublisher = eventPublisher;
    }

    @Transactional
    public UUID placeOrder(String customerId, double amount) {
        UUID orderId = UUID.randomUUID();
        OrderEntity entity = new OrderEntity(orderId, customerId, amount, "CONFIRMED");
        orderRepository.save(entity);

        // Published event is automatically recorded in EVENT_PUBLICATION table!
        eventPublisher.publishEvent(new OrderPlacedEvent(orderId, customerId, amount, Instant.now()));

        return orderId;
    }
}
```

### Example 3: Consuming Module with `@ApplicationModuleListener` in `inventory`
```java
package com.example.platform.inventory.internal;

import com.example.platform.order.OrderPlacedEvent;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.modulith.events.ApplicationModuleListener;
import org.springframework.stereotype.Component;

@Component
class InventoryEventListener {

    private static final Logger log = LoggerFactory.getLogger(InventoryEventListener.class);

    // @ApplicationModuleListener automatically provides:
    // 1. @Async execution
    // 2. @TransactionalEventListener(phase = AFTER_COMMIT)
    // 3. Isolated transaction boundary
    // 4. Guaranteed completion tracking in Event Publication Registry
    @ApplicationModuleListener
    void onOrderPlaced(OrderPlacedEvent event) {
        log.info("Inventory module received OrderPlacedEvent for order: {}. Reserving stock...",
                event.orderId());
        
        // Execute stock reservation
    }
}
```

### Example 4: Leaking Internal Implementation Packages (Anti-Pattern Fix)
```java
// ANTI-PATTERN: Another module importing internal implementation classes:
// package com.example.platform.inventory;
// import com.example.platform.order.internal.OrderEntity; // ARCHITECTURAL VIOLATION!
// Breaks modular encapsulation and couples database schema to foreign modules.

// FIX: Run Spring Modulith verification test in CI pipeline to fail the build immediately!
package com.example.platform;

import org.junit.jupiter.api.Test;
import org.springframework.modulith.core.ApplicationModules;
import org.springframework.modulith.docs.Documenter;

class ModularityArchitectureTest {

    private final ApplicationModules modules = ApplicationModules.of(Application.class);

    @Test
    void verifyModularArchitecture() {
        // Enforces package visibility rules; fails build if illegal imports exist
        modules.verify();
    }

    @Test
    void generateDocumentation() {
        // Automatically creates PlantUML and AsciiDoc diagrams in target/spring-modulith-docs
        new Documenter(modules).writeDocumentation();
    }
}
```

### Example 5: Reprocessing Incomplete Events on Startup
```java
package com.example.platform.config;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.boot.ApplicationRunner;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.modulith.events.IncompleteEventPublications;

import java.time.Duration;

@Configuration
public class EventResilienceConfig {

    private static final Logger log = LoggerFactory.getLogger(EventResilienceConfig.class);

    @Bean
    public ApplicationRunner reprocessStaleEvents(IncompleteEventPublications publications) {
        return args -> {
            log.info("Scanning for incomplete event publications older than 5 minutes...");
            // Scans EVENT_PUBLICATION table for uncompleted events and redelivers them to listeners
            publications.resubmitIncompletePublicationsOlderThan(Duration.ofMinutes(5));
        };
    }
}
```

---

## 6. Best Practices

1. **Keep Module Roots Clean**: Only expose domain service interfaces and public event records in the root of the module package; place repositories, entities, and internal helpers in an `internal` sub-package.
2. **Verify Architecture in CI via Unit Tests**: Always include `ApplicationModules.of(App.class).verify()` in test suites; it runs in milliseconds and acts as an unyielding architectural gatekeeper.
3. **Use `@ApplicationModuleListener` Over Raw `@EventListener`**: Leverage `@ApplicationModuleListener` to guarantee post-commit execution, asynchronous processing, and outbox persistence automatically.
4. **Prefer Event-Driven Communication for Cross-Module Actions**: Instead of `OrderService` directly calling `InventoryService`, publish domain events; this eliminates bidirectional coupling and circular dependencies.
5. **Generate Living Documentation Automatically**: Integrate `new Documenter(modules).writeDocumentation()` into test runs to keep architectural C4 and component diagrams continuously updated in Git.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Importing classes from another module's `internal` package | Violates encapsulation; `modules.verify()` test fails compilation/test | Expose clean interface or event record in root of the target module. |
| Circular dependencies between modules (`A -> B` and `B -> A`) | Destroys modularity and creates deadlocks; fails `verify()` | Decouple one direction by publishing asynchronous domain events. |
| Forgetting to add `spring-modulith-starter-jdbc` | Event Publication Registry is disabled; events are not persisted across crashes | Add `spring-modulith-starter-jdbc` and appropriate schema DDL. |
| Placing non-module classes directly under root package | Confuses Spring Modulith module auto-detection engine | Ensure all domain code is organized into first-level sub-packages. |
| Executing long synchronous tasks in event listeners | Blocks event processing pipeline | `@ApplicationModuleListener` is asynchronous by default, preventing blocking. |
| Missing `@SpringBootApplication` on verification target | Spring Modulith cannot locate the application root package | Pass the `@SpringBootApplication` class into `ApplicationModules.of(...)`. |

---

## 8. Performance & Security Notes

- **Zero Runtime Overhead Verification**: `ApplicationModules.verify()` runs strictly during unit tests and build phases. At runtime, performance is identical to standard Spring Boot execution.
- **Transactional Registry Overhead**: Persisting events to the `EVENT_PUBLICATION` table adds a single local SQL `INSERT` inside the already-open database transaction ($\approx 1\text{ms}$).
- **Database Partitioning**: The `EVENT_PUBLICATION` table can be cleared of completed rows via scheduled jobs or kept as a durable audit log.

---

## 9. Comparison / When to Use What

| Dimension | Spring Modulith (Modular Monolith) | Microservices Architecture | Unstructured Monolith |
|---|---|---|---|
| **Deployment Complexity** | 1 Artifact, 1 Process, 1 DB | Dozens of containers & pipelines | 1 Artifact, 1 Process, 1 DB |
| **Architectural Boundaries** | Strictly enforced via ArchUnit | Enforced by network interfaces | Non-existent (Spaghetti code) |
| **Distributed Latency** | Zero (In-memory event dispatch) | High (Network hops, REST, gRPC) | Zero |
| **Transactional Consistency** | Local ACID transactions | Distributed (Sagas / 2PC) | Local ACID transactions |
| **Event Outbox** | Native built-in zero-config table | Requires Debezium / Kafka | None |
| **Best For** | 90% of enterprise applications | High-scale distinct teams/orgs | Small prototypes only |

---

## 10. Interview Questions

<details>
<summary>1. What is Spring Modulith and what core problem does it solve?</summary>
Spring Modulith is a framework for building well-structured Modular Monoliths in Spring Boot. It solves the architectural degradation of monolithic codebases by enforcing strict package boundaries, preventing illegal cross-module dependencies via ArchUnit verification, automating living architectural documentation, and providing a persistent event publication registry for decoupled inter-module messaging.
</details>

<details>
<summary>2. How does Spring Modulith distinguish between public APIs and internal module implementations?</summary>
Spring Modulith treats any class directly inside the root package of a module (e.g., `com.example.app.order`) as part of that module's public API. Classes residing in nested sub-packages (e.g., `com.example.app.order.internal`) are considered internal and package-private to that module. Attempting to reference an internal class from another module will cause `ApplicationModules.verify()` to fail.
</details>

<details>
<summary>3. What is the Event Publication Registry in Spring Modulith?</summary>
The Event Publication Registry is a built-in Transactional Outbox implementation for in-process events. When a domain event is published during an active transaction, Spring Modulith writes the event into an `EVENT_PUBLICATION` database table in the same transaction. Listeners annotated with `@ApplicationModuleListener` mark the event as completed only after successful execution. If the server crashes, uncompleted events can be re-published on restart.
</details>

<details>
<summary>4. How does `@ApplicationModuleListener` differ from standard `@EventListener`?</summary>
`@ApplicationModuleListener` is a composite annotation that combines:
1. `@Async` (executing on a separate background thread).
2. `@TransactionalEventListener(phase = TransactionPhase.AFTER_COMMIT)`.
3. `@Transactional(propagation = Propagation.REQUIRES_NEW)` (isolated transaction).
4. Automated participation in Spring Modulith's Event Publication Registry.
</details>

<details>
<summary>5. How does Spring Modulith verify architectural boundaries?</summary>
Using ArchUnit under the hood. The developer writes a test invoking `ApplicationModules.of(Application.class).verify()`. ArchUnit analyzes bytecode to ensure modules only access public APIs of other modules, validates that no cyclic dependencies exist between modules, and checks that dependency arrows follow allowed directional flows.
</details>

<details>
<summary>6. How can Spring Modulith generate living documentation?</summary>
By using the `Documenter` class: `new Documenter(modules).writeDocumentation()`. During test execution, it inspects the modules and writes PlantUML component diagrams, C4 architecture models, and AsciiDoc canvases describing module interactions, exported beans, and published events directly to the build output directory.
</details>

<details>
<summary>7. What happens if an event listener fails in Spring Modulith?</summary>
The event publication entry in the `EVENT_PUBLICATION` table remains in an uncompleted state (with completion date `NULL`). Using `IncompleteEventPublications`, the application can scan for and redeliver failed or pending events on application restart or via scheduled background jobs.
</details>

<details>
<summary>8. Why is Spring Modulith considered a stepping stone to Microservices?</summary>
Because it enforces strict domain decoupling, independent module data ownership, and event-driven communication inside a single codebase. If a module subsequently requires independent horizontal scaling or team isolation, extracting it into a microservice is straightforward because its public interfaces and event schemas are already strictly decoupled.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1 (Easy): Structure two Spring Modulith packages</summary>
Create two packages `com.example.app.customer` and `com.example.app.billing`, and place a public record `CustomerCreatedEvent` in the customer root.
</details>

<details>
<summary>Exercise 2 (Easy): Write an architectural verification test</summary>
Write a JUnit 5 test using `ApplicationModules.of(App.class).verify()` and verify that it passes.
</details>

<details>
<summary>Exercise 3 (Medium): Implement @ApplicationModuleListener</summary>
Implement a listener in the `billing` module that consumes `CustomerCreatedEvent` using `@ApplicationModuleListener`.
</details>

<details>
<summary>Exercise 4 (Medium): Test architectural failure on illegal import</summary>
Intentionally import an internal class from `customer.internal` into `billing`, run `verify()`, and assert that the test fails with an architectural violation error.
</details>

<details>
<summary>Exercise 5 (Hard): Generate PlantUML and C4 diagrams</summary>
Configure `Documenter` in a test to generate PlantUML component diagrams and inspect the generated canvas files.
</details>

<details>
<summary>Exercise 6 (Hard): Event Publication Registry resilience test</summary>
Configure `spring-modulith-starter-jdbc` with H2/Postgres, publish an event, simulate a listener failure, and verify that the event remains uncompleted in the `EVENT_PUBLICATION` table.
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                         Spring Modulith Cheat Sheet                             |
+---------------------------------------------------------------------------------+
| Architecture:       Modular Monolith (1 artifact, strict domain boundaries)     |
| Package Rule:       Root package = Public API; Sub-packages = Internal/Private  |
| Verification Test:  ApplicationModules.of(App.class).verify() (ArchUnit)        |
| Event Listener:     @ApplicationModuleListener (Async + Post-Commit + Outbox)   |
| Outbox Table:       EVENT_PUBLICATION (spring-modulith-starter-jdbc)            |
| Living Docs:        new Documenter(modules).writeDocumentation()                |
| Reprocessing:       incompleteEventPublications.resubmitIncomplete...           |
| Invariant:          Never allow circular dependencies or cross-internal imports |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Official Spring Modulith Reference Documentation](https://docs.spring.io/spring-modulith/reference/)
- [Spring Modulith GitHub Repository & Samples](https://github.com/spring-projects/spring-modulith)
- [Monolith to Microservices (Sam Newman)](https://www.oreilly.com/library/view/monolith-to-microservices/9781492047834/)
