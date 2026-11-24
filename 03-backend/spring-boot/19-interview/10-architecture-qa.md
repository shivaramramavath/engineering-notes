# 10: Software Architecture & Design Patterns Technical Interview Q&A

| Metadata Field | Value |
|---|---|
| **Topic** | Hexagonal Architecture, Modular Monoliths vs Microservices, DDD & CQRS |
| **Spring Boot Version** | 3.4.x |
| **Java Version** | 21 / 25 LTS |
| **Target Level** | Senior / Lead / Staff Backend Engineer |
| **Key Focus Areas** | Ports & Adapters, Spring Modulith, DDD Aggregates, CQRS Trade-Offs |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [Internals & Architecture](#3-internals--architecture)
4. [Syntax & Practical Usage](#4-syntax--practical-usage)
5. [5 Practical Examples](#5-5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Pitfalls & Anti-Patterns](#7-common-pitfalls--anti-patterns)
8. [Performance & Security Considerations](#8-performance--security-considerations)
9. [Comparison / Decision Matrix](#9-comparison--decision-matrix)
10. [Real-World Interview Q&A](#10-real-world-interview-qa)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference Cheatsheet](#12-summary--quick-reference-cheatsheet)
13. [Further Reading & Official Resources](#13-further-reading--official-resources)

---

## 1. What & Why

Staff and Lead engineers are judged not just by their ability to write code, but by how they design evolvable, maintainable systems that survive team growth and business pivots.

In senior architecture interviews, discussion focuses on structural trade-offs:
- **Layered Architecture vs. Hexagonal (Ports and Adapters) Architecture**: How the Dependency Inversion Principle (DIP) isolates core business domain logic from database drivers and external HTTP clients.
- **Modular Monolith (Spring Modulith) vs. Microservices**: Understanding when microservices become an operational liability, and how modular monoliths provide strict bounded contexts with monolithic deployment simplicity.
- **Domain-Driven Design (DDD)**: Aggregate Roots, Value Objects, and Domain Events.
- **CQRS (Command Query Responsibility Segregation)**: Splitting read and write models when read volumes dwarf write contention.

---

## 2. Core Concepts

### 2.1 Hexagonal Architecture (Ports and Adapters)
- **Domain Core**: Pure Java business models, entities, and domain rules. Has zero dependencies on Spring Framework, Hibernate, or external libraries.
- **Ports (Interfaces)**:
  - *Inbound (Driver) Ports*: Use cases exposed to the outside world (e.g. `PlaceOrderUseCase`).
  - *Outbound (Driven) Ports*: Interfaces required by the domain to fetch data or emit events (e.g. `OrderRepositoryPort`, `PaymentGatewayPort`).
- **Adapters**: Concrete implementations bridging framework technologies to ports (e.g., `OrderJpaAdapter` implements `OrderRepositoryPort`; `OrderRestController` calls `PlaceOrderUseCase`).

### 2.2 Modular Monoliths with Spring Modulith
A Modular Monolith structures an application into well-defined business modules inside a single deployment unit. **Spring Modulith** enforces module boundaries at build time using ArchUnit: package-private encapsulation prevents Module A from directly calling internal implementation classes of Module B.

---

## 3. Internals & Architecture

```
                      [ Outside World / HTTP / Kafka / CLI ]
                                         │
                                         ▼
   ┌────────────────────────────────────────────────────────────────────────┐
   │                       Primary / Inbound Adapters                       │
   │           Web REST Controller (Spring MVC) / Kafka Listener            │
   └─────────────────────────────────────┬──────────────────────────────────┘
                                         │ Calls Inbound Port
                                         ▼
   ┌────────────────────────────────────────────────────────────────────────┐
   │                               Domain Core                              │
   │  ┌──────────────────────────────────────────────────────────────────┐  │
   │  │ Inbound Port: PlaceOrderUseCase                                  │  │
   │  └──────────────────────────────────┬───────────────────────────────┘  │
   │                                     ▼                                  │
   │  ┌──────────────────────────────────────────────────────────────────┐  │
   │  │ Domain Entities & Aggregate Roots (Order, OrderItem, Money)      │  │
   │  │ Pure Java Business Invariants (Zero Spring/Hibernate Imports)    │  │
   │  └──────────────────────────────────┬───────────────────────────────┘  │
   │                                     ▼                                  │
   │  ┌──────────────────────────────────────────────────────────────────┐  │
   │  │ Outbound Port: OrderRepositoryPort / PaymentGatewayPort          │  │
   │  └──────────────────────────────────┬───────────────────────────────┘  │
   └─────────────────────────────────────┼──────────────────────────────────┘
                                         │ Implemented by Outbound Adapter
                                         ▼
   ┌────────────────────────────────────────────────────────────────────────┐
   │                      Secondary / Outbound Adapters                     │
   │       Spring Data JPA Repository Adapter / Stripe REST Gateway Adapter │
   └────────────────────────────────────────────────────────────────────────┘
```

---

## 4. Syntax & Practical Usage

### 4.1 Dependency Inversion in Hexagonal Architecture
```java
// Pure Domain Port (Domain Core)
public interface OrderRepositoryPort {
    void save(Order order);
}

// Concrete JPA Adapter (Infrastructure Layer)
@Repository
public class OrderJpaAdapter implements OrderRepositoryPort {
    private final SpringDataOrderJpaRepository jpaRepo;
    public void save(Order order) { ... }
}
```

---

## 5. 5 Practical Examples

### Example 1: Pure Domain Aggregate Root (Zero Framework Imports)

```java
package com.example.architecture.domain;

import java.math.BigDecimal;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.UUID;

// Pure Java Domain Entity: No JPA, no Spring annotations!
public class OrderAggregate {

    private final UUID orderId;
    private final String customerId;
    private final List<OrderItem> items = new ArrayList<>();
    private OrderStatus status;

    public enum OrderStatus { CREATED, PAID, SHIPPED, CANCELLED }

    public OrderAggregate(UUID orderId, String customerId) {
        if (orderId == null || customerId == null) {
            throw new IllegalArgumentException("OrderId and CustomerId are mandatory");
        }
        this.orderId = orderId;
        this.customerId = customerId;
        this.status = OrderStatus.CREATED;
    }

    public void addItem(String sku, int quantity, BigDecimal unitPrice) {
        if (status != OrderStatus.CREATED) {
            throw new IllegalStateException("Cannot add items to an order that is already " + status);
        }
        items.add(new OrderItem(sku, quantity, unitPrice));
    }

    public void markAsPaid() {
        if (items.isEmpty()) {
            throw new IllegalStateException("Cannot pay for an empty order");
        }
        this.status = OrderStatus.PAID;
    }

    public UUID getOrderId() { return orderId; }
    public OrderStatus getStatus() { return status; }
    public List<OrderItem> getItems() { return Collections.unmodifiableList(items); }
}

record OrderItem(String sku, int quantity, BigDecimal unitPrice) {}
```

### Example 2: Inbound and Outbound Ports

```java
package com.example.architecture.port.inbound;

import com.example.architecture.domain.OrderAggregate;
import java.math.BigDecimal;
import java.util.UUID;

public interface PlaceOrderUseCase {
    OrderAggregate execute(String customerId, String sku, int quantity, BigDecimal price);
}
```

```java
package com.example.architecture.port.outbound;

import com.example.architecture.domain.OrderAggregate;
import java.util.Optional;
import java.util.UUID;

public interface OrderRepositoryPort {
    OrderAggregate save(OrderAggregate order);
    Optional<OrderAggregate> findById(UUID orderId);
}
```

### Example 3: Domain Service Implementing Inbound Port

```java
package com.example.architecture.service;

import com.example.architecture.domain.OrderAggregate;
import com.example.architecture.port.inbound.PlaceOrderUseCase;
import com.example.architecture.port.outbound.OrderRepositoryPort;

import java.math.BigDecimal;
import java.util.UUID;

// Pure Domain Service: Depends solely on domain abstractions
public class PlaceOrderDomainService implements PlaceOrderUseCase {

    private final OrderRepositoryPort orderRepository;

    public PlaceOrderDomainService(OrderRepositoryPort orderRepository) {
        this.orderRepository = orderRepository;
    }

    @Override
    public OrderAggregate execute(String customerId, String sku, int quantity, BigDecimal price) {
        OrderAggregate order = new OrderAggregate(UUID.randomUUID(), customerId);
        order.addItem(sku, quantity, price);
        return orderRepository.save(order);
    }
}
```

### Example 4: Secondary (Outbound) JPA Adapter

```java
package com.example.architecture.adapter.outbound;

import com.example.architecture.domain.OrderAggregate;
import com.example.architecture.port.outbound.OrderRepositoryPort;
import org.springframework.stereotype.Component;

import java.util.Optional;
import java.util.UUID;

@Component
public class OrderJpaPersistenceAdapter implements OrderRepositoryPort {

    private final SpringDataOrderRepository jpaRepository;

    public OrderJpaPersistenceAdapter(SpringDataOrderRepository jpaRepository) {
        this.jpaRepository = jpaRepository;
    }

    @Override
    public OrderAggregate save(OrderAggregate domainOrder) {
        // Map pure domain entity to JPA entity
        OrderJpaEntity entity = new OrderJpaEntity(domainOrder.getOrderId(), domainOrder.getStatus().name());
        jpaRepository.save(entity);
        return domainOrder;
    }

    @Override
    public Optional<OrderAggregate> findById(UUID orderId) {
        return jpaRepository.findById(orderId)
                .map(e -> new OrderAggregate(e.getId(), "cust-123"));
    }
}
```

### Example 5: Spring Modulith Architecture Verification Test

```java
package com.example.architecture;

import org.junit.jupiter.api.Test;
import org.springframework.modulith.core.ApplicationModules;
import org.springframework.modulith.docs.Documenter;

public class ModulithArchitectureRulesTest {

    // Analyzes classpath and parses module packages
    private final ApplicationModules modules = ApplicationModules.of(Application.class);

    @Test
    void shouldEnforceStrictModuleEncapsulation() {
        // Fails if Order module accesses package-private internal classes of Inventory module!
        modules.verify();
    }

    @Test
    void exportC4ComponentDiagrams() {
        new Documenter(modules).writeModulesAsPlantUml();
    }
}
```

---

## 6. Best Practices

- **Zero Framework Annotations in Domain Core**: The domain core should be pure Java (no `@Entity`, `@Table`, or `@Autowired`). This protects core business logic from framework deprecations.
- **Start with a Modular Monolith**: Begin projects as a modular monolith with strict module boundaries. Only extract to microservices when independent team deployment or distinct hardware scaling demands it.
- **Validate Invariants inside the Aggregate Root**: Business logic and rule checks belong inside the Aggregate Root (e.g. `order.markAsPaid()`), not scattered across anemic service classes.
- **Use Domain Events for Inter-Module Decoupling**: When an action in the `order` module needs to trigger behavior in `inventory`, emit an `OrderPlacedEvent` instead of directly calling `InventoryService`.
- **Enforce Architectural Boundaries with Tests**: Use ArchUnit or Spring Modulith's `modules.verify()` in continuous integration to catch illegal package dependencies.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
|---|---|---|
| **Anemic Domain Model** | Entities are dumb bags of getters/setters; business logic is scattered across 50 procedural service classes. | Encapsulate invariants and state transitions inside the Aggregate Root. |
| **Premature Microservice Decomposition** | Small teams split code into 15 microservices; network overhead and operational complexity paralyze development. | Build a Modular Monolith using Spring Modulith first. |
| **Leaking JPA Annotations into Domain Core** | Tightly couples core business logic to Hibernate ORM, making framework upgrades difficult. | Keep domain entities pure; map to separate JPA entities in outbound adapters. |
| **Direct Cross-Module Database Joins** | Module A executes SQL joins across tables owned by Module B, destroying modular encapsulation. | Modules must only communicate via public API interfaces or domain events. |
| **Bidirectional Module Dependencies** | Module A depends on Module B and Module B depends on Module A, creating a circular dependency. | Invert dependencies using ports or emit asynchronous domain events. |
| **Over-Engineering Small Projects** | Implementing full Hexagonal Architecture for a trivial CRUD app creates 5x more files with zero benefit. | Use standard layered architecture for simple CRUD; reserve Hexagonal for complex domains. |

---

## 8. Performance & Security Considerations

### 8.1 In-Memory Modulith Calls vs Network RPCs
A Modular Monolith executes cross-module calls as in-memory JVM invocations ($< 5\text{ nanoseconds}$). Microservices execute calls over HTTP or gRPC ($5-50\text{ milliseconds}$). Monoliths are orders of magnitude faster and immune to network partitions.

### 8.2 Memory Footprint of Port-Adapter Mapping
Mapping between Domain Aggregates, DTOs, and JPA Entities creates short-lived objects. On modern JVMs with Generational ZGC or G1GC, these short-lived objects are reclaimed in the Young Generation with near-zero overhead.

---

## 9. Comparison / Decision Matrix

| Dimension | Layered (3-Tier) | Hexagonal (Ports & Adapters) | Modular Monolith (Modulith) |
|---|---|---|---|
| **Dependency Direction** | Top-down (Controller -> Service -> DB) | Outside-in (Adapters -> Domain) | Bounded module boundaries |
| **Domain Isolation** | Low (Entities carry JPA tags) | Absolute (Pure Java domain) | High (Package-private encapsulation) |
| **Boilerplate Overhead** | Low | High (Ports, adapters, mappers) | Moderate |
| **Best Used For** | Simple CRUD APIs | Complex core enterprise business rules | Enterprise applications with multiple sub-domains |

---

## 10. Real-World Interview Q&A

<details>
<summary><strong>1. Explain Hexagonal (Ports and Adapters) Architecture and its core benefit.</strong></summary>

Hexagonal Architecture isolates the core business domain from external technical concerns (databases, web frameworks, message brokers) using the Dependency Inversion Principle. The domain defines Inbound Ports (use cases) and Outbound Ports (interfaces for persistence/messaging). Adapters implement or call these ports. The primary benefit is maintainability: the core business rules can be tested and evolved without depending on external libraries or frameworks.
</details>

<details>
<summary><strong>2. What is the difference between an Anemic Domain Model and a Rich Domain Model?</strong></summary>

An Anemic Domain Model treats entities as passive data holders with only getters and setters, placing all business validation and state transition logic into procedural service classes. A Rich Domain Model encapsulates business invariants, rules, and state modifications directly within the domain entities (Aggregate Roots).
</details>

<details>
<summary><strong>3. What is Spring Modulith, and why is it gaining popularity over microservices?</strong></summary>

Spring Modulith provides tooling to build well-structured modular monoliths in Spring Boot. It uses package encapsulation and ArchUnit to enforce strict bounded contexts at build time, preventing cross-module code leakage. It offers the architectural cleanliness of microservices without the operational overhead of Kubernetes, network latency, distributed transactions, and multi-service deployments.
</details>

<details>
<summary><strong>4. What is an Aggregate Root in Domain-Driven Design (DDD)?</strong></summary>

An Aggregate Root is the main entity within a cluster of associated objects (an Aggregate) that treats the boundary as a single unit for data changes. External objects are only permitted to hold references to the Aggregate Root, ensuring all business invariants within the aggregate boundary are consistently enforced.
</details>

<details>
<summary><strong>5. When should an engineering team migrate from a Modular Monolith to Microservices?</strong></summary>

Only when organizational or technical drivers require it:
1. Multiple independent teams (50+ engineers) needing decoupled CI/CD deployment cadences.
2. Distinct components having radically different scaling profiles (e.g. video transcoding needing GPU vs user profile CRUD).
3. Hard multi-language technology requirements.
</details>

<details>
<summary><strong>6. What is CQRS (Command Query Responsibility Segregation)?</strong></summary>

CQRS is an architectural pattern that separates operations that mutate state (Commands) from operations that read state (Queries). This allows optimizing write models for business invariants and transactional consistency, while optimizing read models (e.g. denormalized Elasticsearch views or Redis caches) for high-performance querying.
</details>

<details>
<summary><strong>7. How do you prevent Module A from accessing internal classes of Module B in Java?</strong></summary>

In standard Java, mark internal classes as package-private (no access modifier) and expose only public interface ports. With Spring Modulith, automated architecture tests (`modules.verify()`) analyze the bytecode and fail the build if any cross-module package-private violation occurs.
</details>

<details>
<summary><strong>8. What is the role of a Value Object in DDD?</strong></summary>

A Value Object is an immutable domain object defined exclusively by its attributes rather than a persistent identity ID (e.g., `Money`, `Address`, `EmailAddress`). Two value objects with identical attribute values are considered equal.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Create a Pure Domain Value Object</strong></summary>

**Prompt**: Create an immutable `Money` Value Object in Java encapsulating amount and currency with validation.

**Solution**:
```java
public record Money(BigDecimal amount, String currency) {
    public Money {
        if (amount == null || amount.compareTo(BigDecimal.ZERO) < 0) {
            throw new IllegalArgumentException("Amount cannot be negative");
        }
        if (currency == null || currency.length() != 3) {
            throw new IllegalArgumentException("Invalid currency");
        }
    }
}
```
</details>

<details>
<summary><strong>Exercise 2: Implement Dependency Inversion with Outbound Port</strong></summary>

**Prompt**: Define an interface `NotificationPort` in the domain package and implement it in `infrastructure.adapter.EmailNotificationAdapter`.

**Solution**:
Inspect Examples 2 & 4: domain defines interface; adapter implements it using Spring Boot Starter Mail.
</details>

<details>
<summary><strong>Exercise 3: Write Spring Modulith Architecture Test</strong></summary>

**Prompt**: Write a test verifying module structure using `ApplicationModules.of(Application.class).verify()`.

**Solution**:
Inspect Example 5: instantiate `ApplicationModules` and invoke `modules.verify()`.
</details>

<details>
<summary><strong>Exercise 4: Refactor Anemic Service to Rich Domain Model</strong></summary>

**Prompt**: Move business rule `if (account.balance >= amount) account.balance -= amount` from `AccountService` into `Account.withdraw()`.

**Solution**:
Define method `public void withdraw(BigDecimal amount)` inside `Account` entity; throw `InsufficientFundsException` if balance is too low.
</details>

<details>
<summary><strong>Exercise 5: Implement CQRS Separation in Spring Boot</strong></summary>

**Prompt**: Separate a customer service into `CustomerCommandService` (writes) and `CustomerQueryService` (reads with `@Transactional(readOnly = true)`).

**Solution**:
Create two distinct classes: one for mutations and one for queries.
</details>

<details>
<summary><strong>Exercise 6: Test Domain Logic in Pure JUnit without Spring</strong></summary>

**Prompt**: Write a unit test for `OrderAggregate.markAsPaid()` without loading any Spring application context.

**Solution**:
```java
@Test
void shouldThrowWhenPayingEmptyOrder() {
    OrderAggregate order = new OrderAggregate(UUID.randomUUID(), "cust-1");
    assertThrows(IllegalStateException.class, order::markAsPaid);
}
```
</details>

---

## 12. Summary & Quick Reference Cheatsheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                   SOFTWARE ARCHITECTURE CHEATSHEET                     │
├────────────────────────────────────────────────────────────────────────┤
│ 1. Hexagonal: Pure domain core has ZERO framework dependencies         │
│ 2. Ports: Inbound (Use Cases) and Outbound (Persistence/Messaging)    │
│ 3. Modulith: Modular Monolith first; enforce boundaries with ArchUnit  │
│ 4. DDD: Encapsulate business invariants inside Aggregate Roots         │
│ 5. Value Objects: Immutable objects identified by attributes (Money)   │
│ 6. CQRS: Separate write command models from read query projections    │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading & Official Resources

- [Alistair Cockburn: Hexagonal Architecture Explained](https://alistair.cockburn.us/hexagonal-architecture/)
- [Domain-Driven Design: Tackling Complexity in the Heart of Software (Eric Evans)](https://www.domainlanguage.com/ddd/)
- [Spring Modulith Reference Documentation](https://docs.spring.io/spring-modulith/reference/)
- [Martin Fowler: MonolithFirst Strategy](https://martinfowler.com/bliki/MonolithFirst.html)
