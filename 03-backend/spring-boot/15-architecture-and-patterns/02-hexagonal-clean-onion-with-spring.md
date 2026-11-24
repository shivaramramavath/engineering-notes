# Hexagonal, Clean, and Onion Architecture with Spring Boot

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.x |
| **Tested With** | JUnit 5, ArchUnit, MapStruct, Spring Data JPA |

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
Traditional layered architectures suffer from a critical flaw: the **Database-Driven Design** trap. Because the Service layer depends directly on the Persistence/Repository layer, data models (`@Entity` annotated with JPA/Hibernate) inevitably dictate the business logic. Changing a database schema, migrating from PostgreSQL to MongoDB, or replacing an external REST payment provider ripples through every layer of the application. Furthermore, the core business domain becomes tightly coupled to framework-specific annotations (`@Autowired`, `@Transactional`), preventing testability in isolation from the Spring container.

### The Evolution
To solve framework and infrastructure coupling:
- **Hexagonal Architecture (Ports and Adapters)** was introduced by Alistair Cockburn in 2005. It treats the application as an isolated hexagon whose business core interacts with the outside world exclusively through **Ports** (interfaces), implemented by **Adapters** (HTTP, SQL, Messaging).
- **Onion Architecture** (Jeffrey Palermo, 2008) and **Clean Architecture** (Robert C. Martin / Uncle Bob, 2012) reinforced the **Dependency Inversion Principle**: dependencies must always point *inward* toward the pure domain core.

### Production Relevance
Hexagonal and Clean Architectures are the gold standard for high-longevity enterprise systems, mission-critical financial applications, and complex Domain-Driven Design (DDD) projects. They guarantee that core business logic remains 100% testable with pure Java unit tests (executing in milliseconds without Spring context boot overhead).

---

## 2. Core Concepts

### The Hexagonal Architecture Topology

```
+-----------------------------------------------------------------------------------------+
|                                HEXAGONAL (PORTS & ADAPTERS)                             |
+-----------------------------------------------------------------------------------------+
|                                                                                         |
|       [ DRIVING / INBOUND ADAPTERS ]                 [ DRIVEN / OUTBOUND ADAPTERS ]     |
|                                                                                         |
|       +-------------------------+                    +---------------------------+      |
|       |   REST API Controller   |                    |   Spring Data JPA Repo    |      |
|       |   (@RestController)     |                    |   (PostgreSQL Adapter)    |      |
|       +-------------------------+                    +---------------------------+      |
|                    |                                               ^                    |
|                    v                                               |                    |
|         [ Inbound Port / Use Case ]                    [ Outbound Port (SPI) ]          |
|         (Java Interface: PlaceOrder)                   (Java Interface: OrderRepo)      |
|                    |                                               |                    |
|                    +-----------------------+-----------------------+                    |
|                                            |                                            |
|                                            v                                            |
|                                +-----------------------+                                |
|                                |      DOMAIN CORE      |                                |
|                                |  (Pure Java Entities, |                                |
|                                |   Aggregates, Rules,  |                                |
|                                |   Value Objects)      |                                |
|                                +-----------------------+                                |
|                                            ^                                            |
|                    +-----------------------+-----------------------+                    |
|                    |                                               |                    |
|         [ Inbound Port / Use Case ]                    [ Outbound Port (SPI) ]          |
|                    ^                                               |                    |
|                    |                                               v                    |
|       +-------------------------+                    +---------------------------+      |
|       |   Kafka Event Consumer  |                    |   Kafka Event Producer    |      |
|       |   (@KafkaListener)      |                    |   / Stripe Payment Client |      |
|       +-------------------------+                    +---------------------------+      |
|                                                                                         |
+-----------------------------------------------------------------------------------------+
```

### Key Architectural Definitions
1. **Domain Core**: Pure Java POJOs (Entities, Value Objects, Domain Services) that model real-world business concepts. **Contains NO Spring, JPA, or external framework imports.**
2. **Inbound Port (Driving / API)**: Java interface defining what the application can do (Use Cases, e.g., `CreateOrderUseCase`, `CancelOrderUseCase`).
3. **Inbound Adapter (Driving)**: Translates external delivery mechanisms into port invocations (REST Controller, GraphQL Resolver, Kafka Consumer, CLI).
4. **Outbound Port (Driven / SPI)**: Java interface defined *by the domain* specifying what external capabilities it needs (e.g., `SaveOrderPort`, `ChargeCardPort`, `SendNotificationPort`).
5. **Outbound Adapter (Driven)**: Implements an outbound port using concrete technology (PostgreSQL JPA adapter, Stripe REST client, RabbitMQ publisher).

---

## 3. How It Works Internally

### Dependency Inversion & Spring Bean Wiring

```
[ Outside World ]                      [ Domain Core ]                      [ Infrastructure ]
       |                                      |                                     |
RestController (Driving Adapter)              |                                     |
       |                                      |                                     |
       | calls (implements nothing)           |                                     |
       v                                      |                                     |
CreateOrderUseCase (Inbound Port) <-----------+                                     |
       ^                                      |                                     |
       | implemented by                       |                                     |
OrderService (Domain / Application)           |                                     |
       |                                      |                                     |
       | calls                                |                                     |
       v                                      |                                     |
OrderRepositoryPort (Outbound Port) <---------+                                     |
                                                                                    ^
                                                                                    | implements
                                                                    JpaOrderAdapter (Driven Adapter)
                                                                                    |
                                                                                    v
                                                                        Spring Data JpaRepository
```

1. **Compile-time vs Runtime Inversion**:
   - At **compile time**, `OrderService` knows only `OrderRepositoryPort` (an interface in the domain). It has zero knowledge of Hibernate or PostgreSQL.
   - At **runtime**, Spring's `ApplicationContext` scans `@Configuration` and injects the `JpaOrderAdapter` bean into the `OrderService`.
2. **Layer Isolation via Gradle / Maven Multi-Module**:
   - `domain` module: Zero dependencies (only pure Java standard library).
   - `application` module: Depends on `domain`.
   - `infrastructure` module: Depends on `application` and `domain`. Imports Spring Boot, JPA, Jackson, Flyway.

---

## 4. Syntax & Usage

### 1. Inbound Port (Use Case)

```java
package com.example.order.application.port.in;

import com.example.order.domain.OrderId;
import java.math.BigDecimal;
import java.util.UUID;

public interface PlaceOrderUseCase {
    OrderId placeOrder(PlaceOrderCommand command);

    record PlaceOrderCommand(UUID customerId, BigDecimal amount) {
        public PlaceOrderCommand {
            if (amount.compareTo(BigDecimal.ZERO) <= 0) {
                throw new IllegalArgumentException("Amount must be positive");
            }
        }
    }
}
```

### 2. Outbound Port (SPI)

```java
package com.example.order.application.port.out;

import com.example.order.domain.Order;
import com.example.order.domain.OrderId;
import java.util.Optional;

public interface OrderRepositoryPort {
    Order save(Order order);
    Optional<Order> findById(OrderId id);
}
```

### 3. Application Service (Use Case Implementation)

```java
package com.example.order.application.service;

import com.example.order.application.port.in.PlaceOrderUseCase;
import com.example.order.application.port.out.OrderRepositoryPort;
import com.example.order.domain.Order;
import com.example.order.domain.OrderId;

public class PlaceOrderService implements PlaceOrderUseCase {

    private final OrderRepositoryPort orderRepository;

    public PlaceOrderService(OrderRepositoryPort orderRepository) {
        this.orderRepository = orderRepository;
    }

    @Override
    public OrderId placeOrder(PlaceOrderCommand command) {
        Order order = Order.createNew(command.customerId(), command.amount());
        Order saved = orderRepository.save(order);
        return saved.getId();
    }
}
```

---

## 5. Practical Examples

### Example 1: Pure Java Domain Entity (Zero Framework Imports)

```java
package com.example.order.domain;

import java.math.BigDecimal;
import java.time.Instant;
import java.util.Objects;
import java.util.UUID;

public final class Order {
    private final OrderId id;
    private final UUID customerId;
    private final BigDecimal totalAmount;
    private OrderStatus status;
    private final Instant createdAt;

    public Order(OrderId id, UUID customerId, BigDecimal totalAmount, OrderStatus status, Instant createdAt) {
        this.id = Objects.requireNonNull(id, "OrderId must not be null");
        this.customerId = Objects.requireNonNull(customerId, "CustomerId must not be null");
        this.totalAmount = Objects.requireNonNull(totalAmount, "TotalAmount must not be null");
        this.status = Objects.requireNonNull(status, "Status must not be null");
        this.createdAt = Objects.requireNonNull(createdAt, "CreatedAt must not be null");
    }

    public static Order createNew(UUID customerId, BigDecimal amount) {
        return new Order(OrderId.generate(), customerId, amount, OrderStatus.PENDING, Instant.now());
    }

    public void cancel() {
        if (this.status == OrderStatus.SHIPPED || this.status == OrderStatus.DELIVERED) {
            throw new IllegalStateException("Cannot cancel an order that is already " + this.status);
        }
        this.status = OrderStatus.CANCELLED;
    }

    public OrderId getId() { return id; }
    public UUID getCustomerId() { return customerId; }
    public BigDecimal getTotalAmount() { return totalAmount; }
    public OrderStatus getStatus() { return status; }
    public Instant getCreatedAt() { return createdAt; }
}
```

### Example 2: Outbound Persistence Adapter (Mapping JPA Entity to Domain Model)

```java
package com.example.order.infrastructure.adapter.out.persistence;

import com.example.order.application.port.out.OrderRepositoryPort;
import com.example.order.domain.Order;
import com.example.order.domain.OrderId;
import org.springframework.stereotype.Component;

import java.util.Optional;

@Component
public class JpaOrderPersistenceAdapter implements OrderRepositoryPort {

    private final SpringDataOrderRepository springDataRepo;
    private final OrderPersistenceMapper mapper;

    public JpaOrderPersistenceAdapter(SpringDataOrderRepository springDataRepo, OrderPersistenceMapper mapper) {
        this.springDataRepo = springDataRepo;
        this.mapper = mapper;
    }

    @Override
    public Order save(Order order) {
        JpaOrderEntity jpaEntity = mapper.toJpaEntity(order);
        JpaOrderEntity saved = springDataRepo.save(jpaEntity);
        return mapper.toDomainEntity(saved);
    }

    @Override
    public Optional<Order> findById(OrderId id) {
        return springDataRepo.findById(id.value()).map(mapper::toDomainEntity);
    }
}
```

### Example 3: Inbound REST Controller (Driving Adapter)

```java
package com.example.order.infrastructure.adapter.in.web;

import com.example.order.application.port.in.PlaceOrderUseCase;
import com.example.order.application.port.in.PlaceOrderUseCase.PlaceOrderCommand;
import com.example.order.domain.OrderId;
import jakarta.validation.Valid;
import jakarta.validation.constraints.NotNull;
import jakarta.validation.constraints.Positive;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.math.BigDecimal;
import java.net.URI;
import java.util.UUID;

@RestController
@RequestMapping("/api/v1/orders")
public class OrderWebAdapter {

    private final PlaceOrderUseCase placeOrderUseCase;

    public OrderWebAdapter(PlaceOrderUseCase placeOrderUseCase) {
        this.placeOrderUseCase = placeOrderUseCase;
    }

    public record WebPlaceOrderRequest(
        @NotNull UUID customerId,
        @NotNull @Positive BigDecimal amount
    ) {}

    public record WebPlaceOrderResponse(UUID orderId) {}

    @PostMapping
    public ResponseEntity<WebPlaceOrderResponse> placeOrder(@Valid @RequestBody WebPlaceOrderRequest request) {
        PlaceOrderCommand command = new PlaceOrderCommand(request.customerId(), request.amount());
        OrderId orderId = placeOrderUseCase.placeOrder(command);

        return ResponseEntity
            .created(URI.create("/api/v1/orders/" + orderId.value()))
            .body(new WebPlaceOrderResponse(orderId.value()));
    }
}
```

### Example 4: Anti-Pattern (Polluting Domain with JPA Annotations) vs Fixed Clean Separation

#### The Anti-Pattern: Framework Infiltration of Domain Core
```java
// BAD: Domain class directly coupled to Spring, Hibernate, Jackson, and DB schema
@Entity
@Table(name = "orders")
public class PollutedOrder {
    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column(name = "customer_id", nullable = false)
    private UUID customerId;

    @Autowired // HORRIBLE: Injecting Spring bean into an entity!
    @Transient
    private transient PaymentGatewayClient paymentClient;

    public void processPayment() {
        paymentClient.charge(id); // Infrastructure I/O inside domain entity
    }
}
```

#### Production Fix: Explicit Mapping Layer via MapStruct
```java
// MapStruct mapper in infrastructure adapter layer
@Mapper(componentModel = "spring")
public interface OrderPersistenceMapper {

    @Mapping(target = "id", source = "id.value")
    @Mapping(target = "status", expression = "java(domain.getStatus().name())")
    JpaOrderEntity toJpaEntity(Order domain);

    default Order toDomainEntity(JpaOrderEntity jpa) {
        return new Order(
            new OrderId(jpa.getId()),
            jpa.getCustomerId(),
            jpa.getTotalAmount(),
            OrderStatus.valueOf(jpa.getStatus()),
            jpa.getCreatedAt()
        );
    }
}
```

### Example 5: Clean Architecture Verification via ArchUnit

```java
package com.example.architecture;

import com.tngtech.archunit.junit.AnalyzeClasses;
import com.tngtech.archunit.junit.ArchTest;
import com.tngtech.archunit.lang.ArchRule;

import static com.tngtech.archunit.library.Architectures.onionArchitecture;

@AnalyzeClasses(packages = "com.example.order")
public class HexagonalArchitectureComplianceTest {

    @ArchTest
    public static final ArchRule onion_architecture_is_strictly_maintained = onionArchitecture()
        .domainModels("..domain..")
        .domainServices("..domain.service..")
        .applicationServices("..application..")
        .adapter("web", "..infrastructure.adapter.in.web..")
        .adapter("persistence", "..infrastructure.adapter.out.persistence..")
        .adapter("messaging", "..infrastructure.adapter.out.messaging..");
}
```

---

## 6. Best Practices

1. **Keep Domain Module Pure**: The domain package/module should have zero third-party dependencies. No Jackson annotations, no Lombok, no Spring annotations (`@Component`, `@Service`).
2. **Wire Domain Services via `@Configuration`**: Instead of putting `@Service` on domain use cases, declare them explicitly in an infrastructure Spring `@Configuration` class (`@Bean PlaceOrderUseCase placeOrderUseCase(...)`).
3. **Separate DTOs, Domain Models, and Entities**: Maintain three separate representations: Web Request DTO $\to$ Domain Aggregate $\to$ JPA Database Entity.
4. **Use Java Records for Ports / Commands**: Model Use Case input commands as immutable Java `record`s with defensive validation in compact constructors.
5. **Never Let JPA Entities Leak into Use Cases**: Inbound controllers and Outbound adapters must always map into and out of pure domain models.
6. **Enforce Boundaries with Multi-Module Builds**: Use Maven or Gradle multi-module projects (`order-domain`, `order-application`, `order-infrastructure`) where the domain module physically cannot import Spring Boot.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| **Putting `@Entity` on Domain Model** | Ties domain logic to Hibernate lifecycles, lazy loading proxies, and database schema constraints. | Create distinct `JpaOrderEntity` and map via MapStruct in the persistence adapter. |
| **Annotating Domain Services with `@Service`** | Introduces Spring Framework dependency into pure business domain. | Instantiate domain services via `@Bean` in an infrastructure `@Configuration` class. |
| **Bypassing Inbound Ports** | REST Controller calls Outbound Repository directly, skipping business logic validation. | All incoming requests must route through an Inbound Port (Use Case interface). |
| **Domain Calling External APIs Directly** | Violates Dependency Inversion; domain becomes untestable without network mocks. | Define an Outbound Port interface in the domain; implement the HTTP client in an outbound adapter. |
| **Over-engineering simple CRUD** | Implementing 6 layers of mappers and ports for a simple table lookup adds massive boilerplate without domain value. | Use classic Layered Architecture for simple CRUD; reserve Hexagonal for complex business domains. |
| **Sharing DTOs Across Inbound and Outbound** | Inbound API changes unexpectedly break outbound database schema or event messaging schemas. | Isolate DTOs strictly per adapter. |

---

## 8. Performance & Security Notes

### Performance
- **Object Allocation Overhead**: Mapping between Web DTO $\leftrightarrow$ Domain Model $\leftrightarrow$ JPA Entity creates short-lived objects. On modern JVMs (Java 21+), the Garbage Collector (ZGC, G1) easily reclaims these in Young Gen via Escape Analysis with virtually zero overhead.
- **Lazy Loading Elimination**: Because domain models are pure POJOs without Hibernate proxies, `LazyInitializationException` is completely eliminated. All required data is fetched upfront by the persistence adapter.

### Security
- **Defense in Depth**: Domain invariants cannot be bypassed by malicious HTTP payloads. Even if Bean Validation (`@Valid`) fails to catch an invalid state in the web layer, the Domain constructor throws an exception.
- **Zero Framework Vulnerability in Core**: Zero-day vulnerabilities in Spring Framework or Jackson do not affect the internal business logic execution.

---

## 9. Comparison / When to Use What

| Dimension | Classic 3-Tier Layered | Hexagonal (Ports & Adapters) | Clean / Onion Architecture |
|---|---|---|---|
| **Core Center** | Database / Tables | Use Cases / Business Domain | Domain Entities & Enterprise Rules |
| **Spring Coupling** | High (Everywhere) | Isolated to Adapters | Isolated to Outermost Circle |
| **Unit Testing** | Requires `@SpringBootTest` / Mocks | Fast, Pure Java Unit Tests (No Spring) | Fast, Pure Java Unit Tests (No Spring) |
| **Boilerplate** | Low | Medium-High (Ports + Adapters + Mappers) | High (Strict boundary converters) |
| **Suitable Projects** | CRUD, MVPs, Internal tools | Long-lived systems, multi-cloud, DDD | Large enterprise monoliths, banking, medical |
| **Team Skill Required** | Standard Spring developer | Moderate (DDD & Inversion mindset) | Advanced architectural discipline |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>1. What is the fundamental difference between an Inbound Port and an Outbound Port?</strong></summary>

- **Inbound Port (Driving / API)**: Defines the interface through which the outside world interacts with the core application. It represents a **Use Case** (e.g., `PlaceOrderUseCase`, `GetUserProfileQuery`). The domain service *implements* the Inbound Port, and driving adapters (like `@RestController`) *call* it.
- **Outbound Port (Driven / SPI)**: Defines the interface through which the core application interacts with external systems. It represents a **Capability Requirement** (e.g., `OrderRepositoryPort`, `PaymentGatewayPort`). The domain service *calls* the Outbound Port, and driven adapters (like `JpaOrderAdapter`, `StripeClientAdapter`) *implement* it.
</details>

<details>
<summary><strong>2. How do you register pure domain services as Spring Beans without using `@Service` or `@Component` annotations?</strong></summary>

By using an explicit Spring `@Configuration` class located inside the infrastructure module:
```java
@Configuration
public class OrderDomainConfiguration {

    @Bean
    public PlaceOrderUseCase placeOrderUseCase(OrderRepositoryPort orderRepository) {
        // Pure Java instantiation with dependency injection via constructor
        return new PlaceOrderService(orderRepository);
    }
}
```
This keeps the domain code completely free of Spring imports while allowing Spring Boot's dependency injection container to manage its lifecycle.
</details>

<details>
<summary><strong>3. Why does Hexagonal Architecture recommend separate classes for JPA Entities and Domain Models?</strong></summary>

JPA entities are constrained by relational database requirements:
- They require no-arg constructors (often breaking invariants).
- They use mutable fields and getters/setters for ORM hydration.
- They rely on Hibernate bytecode proxies and lazy-loading magic.
In contrast, Domain Models must enforce business invariants at all times:
- Immutable fields where possible.
- Rich domain methods (`order.cancel()`) instead of arbitrary setters.
- No dependency on ORM annotations.
Separating them ensures that database refactorings (e.g., normalizing columns) do not break business logic.
</details>

<details>
<summary><strong>4. How does Hexagonal Architecture support multi-channel inputs (e.g., REST, CLI, and Kafka)?</strong></summary>

Because all input mechanisms target the identical Inbound Port (`PlaceOrderUseCase`):
- `OrderRestController` receives an HTTP POST, maps JSON to `PlaceOrderCommand`, and calls `useCase.placeOrder()`.
- `OrderKafkaConsumer` receives an Apache Kafka event, deserializes Avro/JSON to `PlaceOrderCommand`, and calls `useCase.placeOrder()`.
- `OrderCommandLineRunner` parses terminal CLI flags, creates `PlaceOrderCommand`, and calls `useCase.placeOrder()`.
The business logic executes identically regardless of the delivery mechanism.
</details>

<details>
<summary><strong>5. What is the "Dependency Inversion Principle" and how does it manifest in Clean Architecture?</strong></summary>

High-level modules should not depend on low-level modules; both should depend on abstractions.
In traditional layering, Business Logic depends on Database Repository (High depends on Low).
In Clean Architecture:
- High-level Domain defines an interface: `OrderRepositoryPort`.
- Low-level Infrastructure implements that interface: `JpaOrderAdapter implements OrderRepositoryPort`.
The dependency arrow is inverted: infrastructure depends on domain, but domain depends on nothing.
</details>

<details>
<summary><strong>6. When is Hexagonal Architecture an anti-pattern or poor choice?</strong></summary>

When applied to simple, data-centric CRUD applications that perform basic database insertions and lookups without rich business logic.
In such systems, mapping between DTO $\to$ Command $\to$ Domain $\to$ JPA Entity $\to$ Database Row adds significant cognitive load and thousands of lines of boilerplate without providing any tangible architectural benefit.
</details>

<details>
<summary><strong>7. How do you manage database transactions (`@Transactional`) in Hexagonal Architecture?</strong></summary>

Two common approaches:
1. **Application Service Annotation**: Place `@Transactional` on the Spring `@Bean` method or on the Application Service class in the application layer (which is allowed to depend on Spring's transaction API).
2. **Transaction Port / Decorator Pattern**: If the application layer must remain 100% framework-agnostic, wrap use-case executions in an infrastructure-level `TransactionalUseCaseExecutor` decorator that delegates inside `TransactionTemplate.execute()`.
</details>

<details>
<summary><strong>8. How does ArchUnit prevent developers from accidentally importing Spring annotations into the domain core?</strong></summary>

By asserting package isolation rules on bytecode:
```java
@ArchTest
public static final ArchRule domain_must_not_depend_on_frameworks = 
    noClasses().that().resideInAPackage("..domain..")
        .should().dependOnClassesThat().resideInAnyPackage("org.springframework..", "jakarta.persistence..");
```
If any developer imports `@Autowired`, `@Entity`, or `@Component` into the domain package, the CI build fails immediately.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Model a Pure Java Domain Value Object</strong></summary>

**Objective**: Create an immutable `Money` Value Object in pure Java with currency validation and addition logic.

**Solution**:
```java
package com.example.domain;

import java.math.BigDecimal;
import java.util.Currency;
import java.util.Objects;

public record Money(BigDecimal amount, Currency currency) {
    public Money {
        Objects.requireNonNull(amount, "Amount required");
        Objects.requireNonNull(currency, "Currency required");
        if (amount.compareTo(BigDecimal.ZERO) < 0) {
            throw new IllegalArgumentException("Money amount cannot be negative");
        }
    }

    public Money add(Money other) {
        if (!this.currency.equals(other.currency)) {
            throw new IllegalArgumentException("Cannot add different currencies: " + this.currency + " vs " + other.currency);
        }
        return new Money(this.amount.add(other.amount), this.currency);
    }
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Define an Inbound and Outbound Port Pair</strong></summary>

**Objective**: Write the interface contracts for an email notification capability.

**Solution**:
```java
// Inbound Port (Use Case)
public interface SendWelcomeEmailUseCase {
    void sendWelcome(UUID customerId, String email);
}

// Outbound Port (SPI)
public interface EmailSenderPort {
    void dispatchEmail(String recipient, String subject, String bodyHtml);
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Implement Outbound Email Adapter using Spring Mail</strong></summary>

**Objective**: Implement `EmailSenderPort` using Spring's `JavaMailSender`.

**Solution**:
```java
@Component
public class SmtpEmailAdapter implements EmailSenderPort {

    private final JavaMailSender mailSender;

    public SmtpEmailAdapter(JavaMailSender mailSender) {
        this.mailSender = mailSender;
    }

    @Override
    public void dispatchEmail(String recipient, String subject, String bodyHtml) {
        SimpleMailMessage message = new SimpleMailMessage();
        message.setTo(recipient);
        message.setSubject(subject);
        message.setText(bodyHtml);
        mailSender.send(message);
    }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Configure Pure Use Case Bean in Spring JavaConfig</strong></summary>

**Objective**: Wire a pure Java domain service into the Spring context without putting `@Service` on the domain class.

**Solution**:
```java
@Configuration
public class DomainBeansConfig {

    @Bean
    public PlaceOrderUseCase placeOrderUseCase(
            OrderRepositoryPort orderRepo, 
            PaymentPort paymentPort, 
            NotificationPort notificationPort) {
        return new PlaceOrderService(orderRepo, paymentPort, notificationPort);
    }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Implement Idempotent Inbound Kafka and Web Adapters</strong></summary>

**Objective**: Show how both a Kafka Consumer and a REST Controller call the exact same `ProcessPaymentUseCase` with idempotency key checking.

**Solution**:
```java
// Common Inbound Port
public interface ProcessPaymentUseCase {
    PaymentResult process(PaymentCommand command);
    record PaymentCommand(String idempotencyKey, BigDecimal amount, String customerId) {}
}

// Adapter 1: REST
@RestController
@RequestMapping("/api/payments")
public class PaymentRestController {
    private final ProcessPaymentUseCase useCase;
    public PaymentRestController(ProcessPaymentUseCase useCase) { this.useCase = useCase; }

    @PostMapping
    public ResponseEntity<PaymentResult> pay(@RequestHeader("X-Idempotency-Key") String key, 
                                            @RequestBody WebPaymentDto dto) {
        return ResponseEntity.ok(useCase.process(new PaymentCommand(key, dto.amount(), dto.customerId())));
    }
}

// Adapter 2: Kafka
@Component
public class PaymentKafkaAdapter {
    private final ProcessPaymentUseCase useCase;
    public PaymentKafkaAdapter(ProcessPaymentUseCase useCase) { this.useCase = useCase; }

    @KafkaListener(topics = "payment-requests")
    public void onMessage(PaymentEvent event) {
        useCase.process(new PaymentCommand(event.eventId(), event.amount(), event.customerId()));
    }
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Build an Automated ArchUnit Architecture Guard</strong></summary>

**Objective**: Write an ArchUnit test asserting that:
1. Domain does not depend on Application or Infrastructure.
2. Application does not depend on Infrastructure.
3. Adapters only communicate through Ports.

**Solution**:
```java
@AnalyzeClasses(packages = "com.example")
public class StrictHexagonalArchitectureTest {

    @ArchTest
    public static final ArchRule domain_has_no_outward_dependencies =
        noClasses().that().resideInAPackage("..domain..")
            .should().dependOnClassesThat().resideInAnyPackage("..application..", "..infrastructure..");

    @ArchTest
    public static final ArchRule application_has_no_infrastructure_dependencies =
        noClasses().that().resideInAPackage("..application..")
            .should().dependOnClassesThat().resideInAPackage("..infrastructure..");
}
```
</details>

---

## 12. Summary & Cheat Sheet

```
+----------------------------------------------------------------------------------------------------+
| HEXAGONAL / CLEAN ARCHITECTURE CHEAT SHEET                                                         |
+----------------------------------------------------------------------------------------------------+
| Component           | Role                                      | Allowed Dependencies             |
+---------------------+-------------------------------------------+----------------------------------+
| Domain Core         | Entities, Aggregates, Value Objects,      | Pure Java Standard Library only  |
|                     | Domain Events, Business Invariants        | (NO Spring, NO JPA, NO Jackson)  |
+---------------------+-------------------------------------------+----------------------------------+
| Inbound Port (API)  | Use Case interfaces (Commands, Queries)   | Domain Core models               |
+---------------------+-------------------------------------------+----------------------------------+
| Outbound Port (SPI) | Infrastructure interface requirements     | Domain Core models               |
|                     | (Repositories, Gateways, Senders)         |                                  |
+---------------------+-------------------------------------------+----------------------------------+
| Inbound Adapter     | HTTP Controllers, Kafka Consumers, CLI    | Calls Inbound Ports              |
+---------------------+-------------------------------------------+----------------------------------+
| Outbound Adapter    | JPA Repositories, Redis, REST Clients     | Implements Outbound Ports        |
+---------------------+-------------------------------------------+----------------------------------+
| Wiring Layer        | @Configuration classes                    | Wires adapters into ports        |
+----------------------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Hexagonal Architecture (Original Article by Alistair Cockburn)](https://alistair.cockburn.us/hexagonal-architecture/)
- [Clean Architecture: A Craftsman's Guide to Software Structure and Design (Robert C. Martin)](https://www.oreilly.com/library/view/clean-architecture-a/9780134494272/)
- [Get Your Hands Dirty on Clean Architecture (Tom Hombergs)](https://reflectoring.io/book/)
- [ArchUnit Testing Guide for Hexagonal and Onion Architectures](https://www.archunit.org/user-guide/html/000_Index.html#_onion_architecture)
- [Spring Modulith: Structuring Spring Boot Applications](https://spring.io/projects/spring-modulith)
