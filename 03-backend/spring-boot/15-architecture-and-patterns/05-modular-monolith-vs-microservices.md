# Modular Monoliths vs Microservices with Spring Boot

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.x |
| **Tested With** | Spring Modulith, ArchUnit, Docker, Testcontainers |

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
During the mid-2010s, the software industry widely adopted microservices as the default architecture for almost every greenfield project. However, distributed microservices introduce massive cognitive and operational overhead: network latency, distributed transactions (Sagas, 2PC), partial network failures, complex observability requirements, CI/CD pipeline proliferation, and challenging local development setups. Teams often ended up building a "Distributed Big Ball of Mud"—inheriting all the complexity of distributed systems without any of the organizational benefits.

### The Evolution
The pendulum has swung back toward **Modular Monoliths (Moduliths)**. A modular monolith provides the clear domain boundaries, team autonomy, and encapsulation of microservices, but runs as a single deployable JVM process with in-memory method invocations, single-database ACID transactions, and zero network serialization penalty.

### Production Relevance
With **Spring Modulith** (officially released with Spring Boot 3.x), Spring developers now have first-class framework support to structure, verify, and document modular monolithic architectures. When a system legitimately outgrows a single process, a well-factored Spring Modulith application can be cleanly decomposed into microservices along established module boundaries in days rather than months.

---

## 2. Core Concepts

### Architectural Topology Comparison

```
+-----------------------------------------------------------------------------------------+
|                  MODULAR MONOLITH (SPRING MODULITH) vs MICROSERVICES                    |
+-----------------------------------------------------------------------------------------+
|                                                                                         |
|  [ MODULAR MONOLITH ]                                [ MICROSERVICES ARCHITECTURE ]     |
|  Single JVM Process / Single Pipeline                Multiple Distributed Containers     |
|                                                                                         |
|  +-------------------------------------------+       +-------------+   +-------------+  |
|  |             Spring Boot App               |       |  Customer   |   |    Order    |  |
|  |                                           |       |   Service   |   |   Service   |  |
|  |  +------------+       +------------+      |       +-------------+   +-------------+  |
|  |  |  Customer  |       |   Order    |      |              |                 |         |
|  |  |   Module   |       |   Module   |      |              |                 |         |
|  |  +------------+       +------------+      |              v (Network/REST)  v (Kafka) |
|  |        ^                    ^             |       +-------------+   +-------------+  |
|  |        | (In-Memory Java)   |             |       | Customer DB |   |  Order DB   |  |
|  |        +---------+----------+             |       +-------------+   +-------------+  |
|  |                  |                        |                                          |
|  |                  v                        |       +-------------+   +-------------+  |
|  |  +-------------------------------------+  |       |  Inventory  |   |   Payment   |  |
|  |  |           Billing Module            |  |       |   Service   |   |   Service   |  |
|  |  +-------------------------------------+  |       +-------------+   +-------------+  |
|  +-------------------------------------------+              |                 |         |
|                     |                                       v                 v         |
|                     v                                +-------------+   +-------------+  |
|          [ Single PostgreSQL DB ]                    | Inventory DB|   | Payment DB  |  |
|          (ACID Local Transactions)                   +-------------+   +-------------+  |
|                                                                                         |
+-----------------------------------------------------------------------------------------+
```

### Key Architectural Definitions
1. **Application Module**: A logical unit of domain functionality exposed via a public API package, while internal implementation details remain package-private (`default` scope).
2. **Module Boundary Verification**: Static analysis guaranteeing that Module A cannot access internal classes of Module B without an explicit public interface.
3. **In-Memory Event Bus**: Decoupled asynchronous module communication using Spring's `ApplicationEventPublisher` and `@ApplicationModuleListener`.
4. **Outbox Event Publication Registry**: Guarantees at-least-once event delivery across modular boundaries without distributed transactions.

---

## 3. How It Works Internally

### Spring Modulith Module Detection and Packaging Rules

```
src/main/java/com/example/app/
├── Application.java               <-- Root Application Class (@SpringBootApplication)
│
├── customer/                      <-- Application Module "customer"
│   ├── CustomerApi.java           <-- Public API (accessible by other modules)
│   ├── CustomerDto.java           <-- Public DTO
│   └── internal/                  <-- Hidden Internal Package (BLOCKED by Modulith)
│       ├── CustomerEntity.java    <-- Persistence details
│       ├── CustomerService.java   <-- Internal business rules
│       └── CustomerRepository.java
│
├── order/                         <-- Application Module "order"
│   ├── OrderApi.java              <-- Public API
│   ├── OrderPlacedEvent.java      <-- Public Domain Event
│   └── internal/                  <-- Hidden Internal Package
│
└── billing/                       <-- Application Module "billing"
    ├── BillingListener.java       <-- Listens to OrderPlacedEvent asynchronously
    └── internal/
```

1. **Root Discovery**: Spring Modulith inspects the top-level packages located immediately beneath the `@SpringBootApplication` annotated class. Each top-level package (`customer`, `order`, `billing`) is recognized as a self-contained **Application Module**.
2. **Encapsulation Enforcement**: By default, only types residing directly in the module's root package are accessible to other modules. Sub-packages (such as `customer.internal` or `customer.data`) are strictly forbidden from being imported by other modules.
3. **ArchUnit Test Automation**: Calling `ApplicationModules.of(Application.class).verify()` parses compiled bytecode and fails the build if any cross-module encapsulation violation exists.

---

## 4. Syntax & Usage

### 1. Defining a Public Module Contract and Domain Event

```java
package com.example.order;

import java.math.BigDecimal;
import java.util.UUID;

// Publicly accessible record representing an integration event
public record OrderPlacedEvent(UUID orderId, UUID customerId, BigDecimal totalAmount) {}
```

### 2. Module Boundary Verification Unit Test

```java
package com.example;

import org.junit.jupiter.api.Test;
import org.springframework.modulith.core.ApplicationModules;
import org.springframework.modulith.docs.Documenter;

class ApplicationModularStructureTest {

    // Analyzes classpath and validates dependency directions
    ApplicationModules modules = ApplicationModules.of(Application.class);

    @Test
    void assertModuleBoundaries() {
        // Enforces: No cycles, no access to other modules' internal packages
        modules.verify();
    }

    @Test
    void renderArchitectureDocumentation() {
        // Generates PlantUML component diagrams and Asciidoc tables
        new Documenter(modules).writeDocumentation();
    }
}
```

---

## 5. Practical Examples

### Example 1: Decoupled Inter-Module Communication via Events

```java
// MODULE 1: ORDER MODULE
package com.example.order.internal;

import com.example.order.OrderPlacedEvent;
import org.springframework.context.ApplicationEventPublisher;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.math.BigDecimal;
import java.util.UUID;

@Service
class OrderInternalService {

    private final ApplicationEventPublisher events;

    OrderInternalService(ApplicationEventPublisher events) {
        this.events = events;
    }

    @Transactional
    public UUID placeOrder(UUID customerId, BigDecimal amount) {
        UUID orderId = UUID.randomUUID();
        // Persist order in local DB...
        
        // Publish event across module boundaries
        events.publishEvent(new OrderPlacedEvent(orderId, customerId, amount));
        return orderId;
    }
}
```

```java
// MODULE 2: BILLING MODULE (Completely decoupled from Order internal classes)
package com.example.billing.internal;

import com.example.order.OrderPlacedEvent;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.modulith.events.ApplicationModuleListener;
import org.springframework.stereotype.Component;

@Component
class BillingEventListener {

    private static final Logger log = LoggerFactory.getLogger(BillingEventListener.class);

    @ApplicationModuleListener
    void on(OrderPlacedEvent event) {
        log.info("Billing module received event for order: id={}, amount={}",
                event.orderId(), event.totalAmount());
        // Execute billing invoice generation asynchronously
    }
}
```

### Example 2: Module Explicit API Definition using `@NamedInterface`

```java
package com.example.customer;

import org.springframework.modulith.NamedInterface;

import java.util.Optional;
import java.util.UUID;

// Exposes a specific public API interface to other modules
@NamedInterface("customer-lookup")
public interface CustomerLookupService {
    Optional<CustomerProfileDto> findCustomer(UUID id);
}
```

In `package-info.java`:
```java
@org.springframework.modulith.ApplicationModule(
    allowedDependencies = {"inventory::inventory-api"}
)
package com.example.customer;
```

### Example 3: Transactional Outbox Event Publication Registry

Spring Modulith includes an automated outbox pattern:

```java
package com.example.config;

import org.springframework.context.annotation.Configuration;
import org.springframework.modulith.events.config.EnablePersistentDomainEvents;

@Configuration
@EnablePersistentDomainEvents // Automatically logs published events to event_publication table
public class EventPublicationConfig {
}
```

Database Schema automatically managed by Spring Modulith:
```sql
CREATE TABLE IF NOT EXISTS event_publication (
    id UUID PRIMARY KEY,
    listener_id VARCHAR(512) NOT NULL,
    event_type VARCHAR(512) NOT NULL,
    serialized_event TEXT NOT NULL,
    publication_date TIMESTAMPTZ NOT NULL,
    completion_date TIMESTAMPTZ
);
```

### Example 4: Anti-Pattern (Distributed Monolith) vs Fixed Modular Architecture

#### The Anti-Pattern: Microservices with Synchronous REST Chaining
```
[ Client ] ---> [ Order Service ] (HTTP)
                       |
                       +---> [ Customer Service ] (HTTP)
                       |
                       +---> [ Inventory Service ] (HTTP)
                       |
                       +---> [ Payment Service ] (HTTP)
```
*Failure Modes*: 
- High latency ($L_{total} = L_{order} + L_{cust} + L_{inv} + L_{pay}$).
- Cascading failures: If Inventory Service is down, Order Service throws 500.
- Distributed 2-phase commit hell: If Payment fails, how do you rollback Inventory?

#### Production Fix: Modular Monolith with Asynchronous In-Memory Boundaries
```java
// Single JVM call with transaction atomicity
@Transactional
public OrderResult placeOrder(CreateOrderCmd cmd) {
    // 1. In-memory check against customer module (0ms network latency)
    customerLookup.assertActive(cmd.customerId());

    // 2. Local database commit
    Order order = orderRepository.save(new Order(cmd));

    // 3. Asynchronous decoupled event to Inventory and Payment
    eventPublisher.publishEvent(new OrderPlacedEvent(order.getId()));

    return new OrderResult(order.getId());
}
```

### Example 5: Clean Path to Decomposition (Extracting a Module to a Microservice)

When the `billing` module needs independent autoscaling:

1. **Step 1 (Inside Monolith)**: Ensure all communication with `billing` happens via `BillingApi` and `OrderPlacedEvent`.
2. **Step 2 (Decomposition)**:
   - Create a new Git repository: `billing-service`.
   - Copy the `com.example.billing` package into the new project.
   - Replace the in-memory `@ApplicationModuleListener` with an Apache `@KafkaListener(topics = "order-events")`.
   - In the monolith, replace the in-memory publisher with a Kafka producer.
3. Because the domain model was already encapsulated, **zero business logic changes** are required!

---

## 6. Best Practices

1. **Start with a Modular Monolith**: Unless you have 50+ engineers in distinct autonomous teams, start with a modular monolith. Microservice complexity kills early-stage velocity.
2. **Verify Boundaries in Continuous Integration**: Run `ApplicationModules.of(Application.class).verify()` on every pull request. Block PRs that introduce illegal module cycles or leaks.
3. **Communicate Asynchronously via Events**: Favor domain events (`ApplicationEventPublisher`) over direct synchronous service bean injection between modules.
4. **Use Package-Private Access Modifiers**: Keep service implementations, entities, and repositories package-private (`default` scope). Expose only public interfaces and DTO records.
5. **Separate Database Schemas per Module**: In PostgreSQL, use schemas (`customer_schema`, `order_schema`) within a single database instance to prevent direct cross-table SQL joins.
6. **Auto-Generate Architecture Diagrams**: Use Spring Modulith's `Documenter` to generate up-to-date C4 and PlantUML component diagrams as part of your Maven/Gradle build.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| **Circular Module Dependencies** | Module A calls Module B, which calls Module A; creates tight coupling and prevents modular testing. | Decouple via Domain Events (`OrderPlacedEvent`) or invert dependencies using an interface. |
| **Direct Cross-Module SQL Joins** | Running SQL queries that join tables from two different modules prevents extracting microservices. | Query only your own module's tables; fetch external data via public Module API or events. |
| **Premature Microservices** | Adopting microservices for a 5-person engineering team creates 80% DevOps overhead and 20% feature output. | Build a clean Modular Monolith using Spring Modulith first. |
| **Making All Classes Public** | Junior developers inject internal repositories across modules, eroding architecture into a Big Ball of Mud. | Keep implementations package-private; expose only interfaces in the module root. |
| **Using Distributed Transactions (XA/2PC)** | Microservices trying to maintain synchronous ACID transactions across databases cause deadlocks and timeouts. | Adopt eventual consistency and the SAGA pattern with compensating transactions. |
| **Ignoring Event Publication Failures** | In-memory events are lost if the JVM crashes before listeners finish. | Use Spring Modulith's Event Publication Registry (Outbox table). |

---

## 8. Performance & Security Notes

### Performance
- **Zero Serialization Penalty**: Method calls between modules are native JVM pointer dereferences taking under 1 nanosecond, compared to 5–25 milliseconds for JSON HTTP/REST network roundtrips in microservices.
- **Resource Footprint**: A modular monolith runs in a single JVM (512MB RAM), whereas 10 microservices require 10 JVM runtimes (5GB+ RAM minimum).

### Security
- **Internal Trust Zone**: Inter-module communication requires no mTLS, network perimeter firewalling, or JWT re-verification on every method call.
- **Unified Security Context**: Spring Security's `SecurityContextHolder` is automatically propagated across modular method invocations without token relay headers.

---

## 9. Comparison / When to Use What

| Dimension | Monolithic Application (Traditional) | Modular Monolith (Spring Modulith) | Distributed Microservices |
|---|---|---|---|
| **Deployment Unit** | Single JAR / WAR | Single JAR | Multiple independent Containers |
| **Domain Boundaries**| Blurred (Spaghetti code) | Strictly Enforced by Framework | Physically Enforced by Network |
| **Inter-Service Comms**| Direct Method Calls | Events & Public Interfaces | REST / gRPC / Kafka |
| **Database Model** | Shared relational tables | Schema-per-module / Logical separation| Database-per-service |
| **DevOps Complexity** | Very Low | Low | Very High (K8s, Istio, OTel) |
| **Team Autonomy** | Low | High (Modules owned by teams) | Maximum (Separate repos & deploys) |
| **Best Used For** | Tiny scripts / Prototypes | 90% of Enterprise Applications | Mega-scale orgs (Amazon, Netflix) |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>1. What is the fundamental difference between a traditional monolith and a modular monolith?</strong></summary>

A traditional monolith is typically structured by technical layers (`controllers/`, `services/`, `repositories/`) where all classes are public. Over time, code cross-references uncontrollably, creating an unmaintainable "Big Ball of Mud".
A **Modular Monolith** is structured strictly by business domain modules (`customer/`, `order/`, `billing/`). Each module strictly encapsulates its internal implementation details using package-private visibility and publishes an explicit public API. Architectural boundaries are verified via automated bytecode analysis (Spring Modulith / ArchUnit).
</details>

<details>
<summary><strong>2. How does Spring Modulith enforce module encapsulation without requiring separate Gradle/Maven submodules?</strong></summary>

Spring Modulith inspects package structures at the JVM level:
- Packages located directly beneath the root package are treated as module roots.
- Classes inside subpackages (e.g., `com.example.order.internal.*`) are marked as hidden.
- When running `ApplicationModules.of(...).verify()`, Spring Modulith evaluates the compiled Java classes using ArchUnit. If a class in `com.example.billing` imports a class from `com.example.order.internal`, the unit test fails with a clear architectural violation report.
</details>

<details>
<summary><strong>3. What is the "Distributed Monolith" anti-pattern and how do teams fall into it?</strong></summary>

A distributed monolith occurs when a team decomposes a system into multiple microservices, but retains tight coupling between them:
- Services communicate via deep synchronous chains of REST calls.
- Deploying a change in Service A requires deploying synchronized changes in Services B, C, and D.
- Databases share tables across network boundaries.
Teams inherit all the operational pains of distributed systems (latency, partial failures, network splits) with none of the benefits of independent team velocity.
</details>

<details>
<summary><strong>4. How does the Spring Modulith Event Publication Registry ensure reliable asynchronous communication?</strong></summary>

When an event is published via `ApplicationEventPublisher`:
1. Spring Modulith's transactional event interceptor records the event payload and its intended listener IDs into an `event_publication` table within the same database transaction.
2. The transaction commits atomically.
3. The `@ApplicationModuleListener` executes. Upon successful completion, the completion timestamp in the `event_publication` table is updated.
4. If the JVM crashes during processing, Spring Modulith provides an automated completion manager that detects incomplete publications upon startup and re-dispatches them.
</details>

<details>
<summary><strong>5. When is it genuinely time to split a module from a modular monolith into an independent microservice?</strong></summary>

Decomposition is justified only when:
1. **Asymmetric Scaling**: The module requires radically different hardware (e.g., GPU nodes for AI inference, or 100x the CPU of the rest of the application).
2. **Independent Deployment Cadence**: A dedicated team needs to release updates 50 times a day without waiting for the main monolith deployment cycle.
3. **Different Technology Stack**: The problem is drastically better solved in Rust or Python (e.g., low-level systems programming or data science).
4. **Strict Compliance / Security**: The module handles PCI-DSS credit card vaults and must be quarantined from general enterprise code.
</details>

<details>
<summary><strong>6. How can database tables be logically separated in a modular monolith to prepare for eventual microservice extraction?</strong></summary>

- **Separate Schemas in a Single Database**: Use PostgreSQL schemas (`CREATE SCHEMA customer; CREATE SCHEMA orders;`).
- Configure module entities to bind strictly to their schema (`@Table(schema = "orders", name = "orders")`).
- Grant separate database user roles per schema.
- Forbid any foreign keys across schemas.
This guarantees that extracting the `orders` module into its own standalone database requires zero DDL changes.
</details>

<details>
<summary><strong>7. How does `@ApplicationModuleListener` differ from standard Spring `@EventListener`?</strong></summary>

`@ApplicationModuleListener` is a composite annotation that combines:
- `@Async`: Executes the listener on a separate background thread (or Virtual Thread).
- `@TransactionalEventListener(phase = TransactionPhase.AFTER_COMMIT)`: Runs only after the publishing database transaction commits.
- Spring Modulith transaction management: Integrates with the Event Publication Registry for guaranteed delivery.
</details>

<details>
<summary><strong>8. What is the impact of Java 21 Virtual Threads on Modular Monolith throughput?</strong></summary>

In traditional microservices, asynchronous non-blocking programming required complex reactive frameworks (Spring WebFlux, Project Reactor).
In a Spring Modulith application running on Java 21 LTS with `spring.threads.virtual.enabled=true`:
- Inter-module asynchronous event listeners execute on lightweight Virtual Threads.
- You write straightforward blocking code with high concurrency and negligible memory footprint, eliminating the need to split into microservices solely for concurrency scaling.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Structure a Modular Package Layout</strong></summary>

**Objective**: Organize a package tree for an `inventory` module following Spring Modulith conventions.

**Solution**:
```
com.example.myapp.inventory
├── InventoryApi.java              <-- Public interface
├── InventoryReservationDto.java   <-- Public DTO
├── package-info.java              <-- Module metadata
└── internal/
    ├── InventoryItem.java         <-- Package-private JPA Entity
    ├── InventoryRepository.java   <-- Package-private Repository
    └── InventoryServiceImpl.java  <-- Package-private Service
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Write a Spring Modulith Verification Test</strong></summary>

**Objective**: Create a JUnit 5 test that validates application modularity and outputs failure reports.

**Solution**:
```java
class ArchitectureTest {
    @Test
    void verifyArchitecture() {
        ApplicationModules.of(Application.class).verify();
    }
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Document Modules with C4 PlantUML Diagrams</strong></summary>

**Objective**: Write a test that generates Component C4 diagrams into `target/spring-modulith-docs`.

**Solution**:
```java
class DocumentationTest {
    @Test
    void createDocs() {
        ApplicationModules modules = ApplicationModules.of(Application.class);
        new Documenter(modules)
            .writeDocumentation(Documenter.DiagramOptions.defaults(), 
                               Documenter.CanvasOptions.defaults());
    }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Implement Asynchronous Module Listener with Virtual Threads</strong></summary>

**Objective**: Configure an `@ApplicationModuleListener` that processes welcome emails on a virtual thread without blocking the user registration transaction.

**Solution**:
In `application.yaml`:
```yaml
spring:
  threads:
    virtual:
      enabled: true
```

In Java:
```java
@Component
public class WelcomeEmailModuleListener {

    private static final Logger log = LoggerFactory.getLogger(WelcomeEmailModuleListener.class);

    @ApplicationModuleListener
    public void onUserRegistered(UserRegisteredEvent event) {
        log.info("Processing welcome email on thread: {}", Thread.currentThread());
        // Pure blocking I/O safely running on Virtual Thread
    }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Implement Module Schema Separation in PostgreSQL</strong></summary>

**Objective**: Configure two distinct JPA entities residing in separate modules to target distinct database schemas, avoiding cross-schema foreign keys.

**Solution**:
```java
// Module: customer
package com.example.customer.internal;

@Entity
@Table(name = "customers", schema = "customer_schema")
class CustomerEntity {
    @Id @GeneratedValue(strategy = GenerationType.UUID)
    private UUID id;
    private String email;
}

// Module: order
package com.example.order.internal;

@Entity
@Table(name = "orders", schema = "order_schema")
class OrderEntity {
    @Id @GeneratedValue(strategy = GenerationType.UUID)
    private UUID id;

    // Referenced by ID, NOT a foreign key object reference!
    @Column(name = "customer_id", nullable = false)
    private UUID customerId;
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Custom Incomplete Event Publication Cleaner</strong></summary>

**Objective**: Write a scheduled task using Spring Modulith's `IncompleteEventPublications` API to republish events that failed during a server restart.

**Solution**:
```java
@Component
public class ResilientEventCleaner {

    private final IncompleteEventPublications incompleteEvents;

    public ResilientEventCleaner(IncompleteEventPublications incompleteEvents) {
        this.incompleteEvents = incompleteEvents;
    }

    @Scheduled(fixedDelay = 60_000)
    public void retryStaleEvents() {
        // Find events older than 5 minutes that never completed and republish
        incompleteEvents.resubmitIncompletePublicationsOlderThan(Duration.ofMinutes(5));
    }
}
```
</details>

---

## 12. Summary & Cheat Sheet

```
+----------------------------------------------------------------------------------------------------+
| MODULAR MONOLITHS (SPRING MODULITH) CHEAT SHEET                                                    |
+----------------------------------------------------------------------------------------------------+
| Concept             | Spring Modulith Implementation            | Primary Benefit                  |
+---------------------+-------------------------------------------+----------------------------------+
| Application Module  | Top-level package under main app          | Clear business cohesion          |
+---------------------+-------------------------------------------+----------------------------------+
| Internal Packages   | `internal/` subpackage                    | Encapsulation; hides components  |
+---------------------+-------------------------------------------+----------------------------------+
| Verification        | `ApplicationModules.of(...).verify()`     | Catches illegal imports in CI/CD |
+---------------------+-------------------------------------------+----------------------------------+
| Inter-Module Events | `@ApplicationModuleListener`              | Decoupled, transactional comms   |
+---------------------+-------------------------------------------+----------------------------------+
| Outbox Registry     | `@EnablePersistentDomainEvents`           | At-least-once event delivery     |
+---------------------+-------------------------------------------+----------------------------------+
| Architecture Docs   | `new Documenter(modules).writeDocs()`     | Generates C4 / PlantUML diagrams |
+---------------------+-------------------------------------------+----------------------------------+
| Extraction Path     | Replace event bus with Kafka listener     | Smooth, risk-free microservices  |
+----------------------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Spring Modulith Reference Documentation](https://docs.spring.io/spring-modulith/reference/)
- [Monolith to Microservices: Evolutionary Patterns to Transform Your Monolith (Sam Newman)](https://www.oreilly.com/library/view/monolith-to-microservices/9781492047834/)
- [Martin Fowler: MonolithFirst Strategy](https://martinfowler.com/bliki/MonolithFirst.html)
- [ArchUnit: Testing Architecture as Code](https://www.archunit.org/)
- [Oliver Drotbohm's Spring Modulith Deep Dive (YouTube)](https://www.youtube.com/watch?v=Ssm_h9Vv8oM)
