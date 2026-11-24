# Domain-Driven Design (DDD) with Spring Boot

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.x |
| **Tested With** | Spring Data JPA, Spring Modulith, jMolecules, PostgreSQL |

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
In enterprise software development, business rules and domain logic often get lost in technical complexity. Database tables, HTTP endpoints, and ORM mapping annotations take precedence over actual domain problems. As business rules become complex (e.g., pricing rules, inventory reservations, multi-stage compliance approvals), procedural code inside Spring `@Service` classes expands into unmaintainable multi-thousand-line methods. Teams suffer from ubiquitous translation errors between business stakeholders and developers.

### The Evolution
**Domain-Driven Design (DDD)**, formulated by Eric Evans in 2003, bridges the gap between software implementation and business reality. It provides:
1. **Strategic Design**: Bounded Contexts, Ubiquitous Language, and Context Mapping to divide large systems into distinct linguistic and operational domains.
2. **Tactical Design**: Concrete programming building blocks (Aggregates, Entities, Value Objects, Domain Events, Repositories, Domain Services) that keep business invariants strictly encapsulated.

### Production Relevance
When building enterprise Spring Boot microservices or modular monoliths (Spring Modulith), DDD patterns prevent domain model erosion. By leveraging modern Java 21 features (Records, Sealed Types, Pattern Matching) alongside Spring Data repositories and Domain Events, teams produce self-documenting code that directly reflects business processes.

---

## 2. Core Concepts

### Tactical DDD Building Blocks in Spring Boot

```
+-----------------------------------------------------------------------------------------+
|                                TACTICAL DDD ARCHITECTURE                                |
+-----------------------------------------------------------------------------------------+
|                                                                                         |
|  +-----------------------------------------------------------------------------------+  |
|  | AGGREGATE BOUNDARY (OrderAggregate)                                              |  |
|  |                                                                                   |  |
|  |   [ AGGREGATE ROOT ] <---------------------+ (Guards Invariants)                  |  |
|  |   Order (Entity with identity)             |                                      |  |
|  |     - OrderId (Value Object)               |                                      |  |
|  |     - CustomerId (Reference)               |                                      |  |
|  |     - Money (Value Object)                 |                                      |  |
|  |     - OrderStatus (Value Object/Enum)      |                                      |  |
|  |                                            |                                      |  |
|  |   [ INTERNAL ENTITIES ]                    |                                      |  |
|  |   OrderItem (Entity)                       |                                      |  |
|  |     - OrderItemId (Identity)               |                                      |  |
|  |     - Quantity (Value Object)              |                                      |  |
|  |     - ProductId (Reference)                |                                      |  |
|  |                                            |                                      |  |
|  |   [ DOMAIN EVENTS ]                        |                                      |  |
|  |   OrderPlacedEvent                         v                                      |  |
|  |   (Emitted upon invariant completion) ----> [ ApplicationEventPublisher ]         |  |
|  +-----------------------------------------------------------------------------------+  |
|                                           |                                             |
|                                           v                                             |
|                              [ ORDER REPOSITORY ]                                       |
|                              (Persists Root ONLY)                                       |
|                                                                                         |
+-----------------------------------------------------------------------------------------+
```

### Key Tactical Definitions
1. **Entity**: An object defined by its persistent identity (`OrderId`), not its attributes. Two orders with the same ID are the same entity even if their items differ.
2. **Value Object**: An immutable object defined entirely by its attributes (e.g., `Money`, `Address`, `Quantity`). It has no independent lifecycle or database ID.
3. **Aggregate & Aggregate Root**: A cluster of associated domain objects treated as a single unit for data changes. External objects may only hold a reference to the **Aggregate Root**. The root enforces all business invariants.
4. **Domain Event**: A record of something meaningful that happened in the domain in the past (e.g., `OrderPlaced`, `PaymentDeclined`).
5. **Repository**: Mechanism providing collection-like semantics to access and persist entire Aggregate Roots. **Never create a repository for an internal entity (e.g., NO `OrderItemRepository`)!**
6. **Domain Service**: Business logic that spans multiple aggregates or does not naturally belong to a single entity (e.g., `CurrencyExchangeService`).

---

## 3. How It Works Internally

### Domain Events Lifecycle with Spring Data & `@DomainEvents`

```
[ Application Service ]
       |
       | 1. order.placeOrder(...)
       v
[ Order Aggregate Root ] (registers OrderPlacedEvent via @DomainEvents)
       |
       | 2. orderRepository.save(order)
       v
[ Spring Data JpaRepository ]
       |
       | 3. Emits events via ApplicationEventPublisher
       v
[ Spring Event Multicaster ]
       |
       +---> [ @EventListener / @TransactionalEventListener ] (Local Handlers)
       |
       +---> [ Outbox / Kafka Publisher ] (Async Integration Events)
```

1. **State Mutation**: The client triggers a command on the Aggregate Root (`order.placeOrder()`).
2. **Event Registration**: The root invokes `registerEvent(new OrderPlacedEvent(this.getId()))`. In Spring Data, extending `AbstractAggregateRoot<T>` provides `registerEvent()`.
3. **Save & Publication**: When `repository.save(order)` is executed, Spring Data automatically captures all registered events, publishes them to Spring's `ApplicationEventPublisher`, and clears the event queue from the aggregate.
4. **Transactional Handling**: Downstream listeners annotated with `@TransactionalEventListener(phase = TransactionPhase.AFTER_COMMIT)` execute only after the database transaction successfully commits.

---

## 4. Syntax & Usage

### 1. Modeling Value Objects with Java Records

```java
package com.example.domain.model;

import java.math.BigDecimal;
import java.util.Currency;
import java.util.Objects;

public record Money(BigDecimal amount, Currency currency) {

    public static final Money ZERO_USD = new Money(BigDecimal.ZERO, Currency.getInstance("USD"));

    public Money {
        Objects.requireNonNull(amount, "Amount cannot be null");
        Objects.requireNonNull(currency, "Currency cannot be null");
        if (amount.compareTo(BigDecimal.ZERO) < 0) {
            throw new IllegalArgumentException("Money cannot be negative: " + amount);
        }
    }

    public static Money of(double amount, String currencyCode) {
        return new Money(BigDecimal.valueOf(amount), Currency.getInstance(currencyCode));
    }

    public Money add(Money other) {
        if (!this.currency.equals(other.currency)) {
            throw new IllegalArgumentException("Currency mismatch: " + this.currency + " vs " + other.currency);
        }
        return new Money(this.amount.add(other.amount), this.currency);
    }
}
```

### 2. Aggregate Root using Spring Data `AbstractAggregateRoot`

```java
package com.example.domain.order;

import com.example.domain.model.Money;
import org.springframework.data.domain.AbstractAggregateRoot;

import java.time.Instant;
import java.util.*;

public class Order extends AbstractAggregateRoot<Order> {

    private final OrderId id;
    private final CustomerId customerId;
    private final List<OrderItem> items = new ArrayList<>();
    private OrderStatus status;
    private Money totalAmount;
    private final Instant createdAt;

    public Order(OrderId id, CustomerId customerId) {
        this.id = Objects.requireNonNull(id);
        this.customerId = Objects.requireNonNull(customerId);
        this.status = OrderStatus.DRAFT;
        this.totalAmount = Money.ZERO_USD;
        this.createdAt = Instant.now();
    }

    public void addItem(ProductId productId, int quantity, Money unitPrice) {
        if (this.status != OrderStatus.DRAFT) {
            throw new IllegalStateException("Cannot add items to an order in " + this.status + " state.");
        }
        OrderItem item = new OrderItem(OrderItemId.generate(), productId, quantity, unitPrice);
        this.items.add(item);
        this.totalAmount = this.totalAmount.add(unitPrice.multiply(quantity));
    }

    public void submit() {
        if (this.items.isEmpty()) {
            throw new IllegalStateException("Cannot submit an empty order.");
        }
        this.status = OrderStatus.SUBMITTED;
        
        // Tactical DDD: Register domain event automatically published on repository.save()
        registerEvent(new OrderSubmittedEvent(this.id, this.customerId, this.totalAmount, Instant.now()));
    }

    public OrderId getId() { return id; }
    public CustomerId getCustomerId() { return customerId; }
    public List<OrderItem> getItems() { return Collections.unmodifiableList(items); }
    public OrderStatus getStatus() { return status; }
    public Money getTotalAmount() { return totalAmount; }
}
```

---

## 5. Practical Examples

### Example 1: Aggregate Root Entity with Encapsulated Invariants

```java
package com.example.domain.inventory;

import java.util.Objects;
import java.util.UUID;

public record ProductId(UUID value) {
    public static ProductId generate() { return new ProductId(UUID.randomUUID()); }
}

public class InventoryItem {
    private final ProductId productId;
    private int availableQuantity;
    private int reservedQuantity;

    public InventoryItem(ProductId productId, int initialStock) {
        this.productId = Objects.requireNonNull(productId);
        if (initialStock < 0) {
            throw new IllegalArgumentException("Initial stock cannot be negative");
        }
        this.availableQuantity = initialStock;
        this.reservedQuantity = 0;
    }

    public void reserve(int count) {
        if (count <= 0) {
            throw new IllegalArgumentException("Reservation count must be positive");
        }
        if (count > availableQuantity) {
            throw new InsufficientInventoryException(productId, count, availableQuantity);
        }
        this.availableQuantity -= count;
        this.reservedQuantity += count;
    }

    public void confirmSale(int count) {
        if (count > reservedQuantity) {
            throw new IllegalStateException("Cannot confirm more than currently reserved");
        }
        this.reservedQuantity -= count;
    }

    public int getAvailableQuantity() { return availableQuantity; }
    public int getReservedQuantity() { return reservedQuantity; }
}
```

### Example 2: Domain Events & `@TransactionalEventListener`

```java
package com.example.order.event;

import com.example.domain.order.OrderId;
import com.example.domain.order.OrderSubmittedEvent;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Component;
import org.springframework.transaction.event.TransactionPhase;
import org.springframework.transaction.event.TransactionalEventListener;

@Component
public class OrderNotificationHandler {

    private static final Logger log = LoggerFactory.getLogger(OrderNotificationHandler.class);

    @TransactionalEventListener(phase = TransactionPhase.AFTER_COMMIT)
    public void handleOrderSubmitted(OrderSubmittedEvent event) {
        // This runs ONLY if the DB transaction committing the order succeeded!
        log.info("Order successfully committed: orderId={}, customerId={}, amount={}",
                event.orderId(), event.customerId(), event.totalAmount());
        
        // Dispatch asynchronous notification or publish to Kafka message broker
    }

    @TransactionalEventListener(phase = TransactionPhase.AFTER_ROLLBACK)
    public void handleOrderRollback(OrderSubmittedEvent event) {
        log.warn("Order transaction rolled back for orderId={}", event.orderId());
    }
}
```

### Example 3: Value Object Mapping with JPA AttributeConverter

```java
package com.example.infrastructure.persistence;

import com.example.domain.model.Money;
import jakarta.persistence.AttributeConverter;
import jakarta.persistence.Converter;

import java.math.BigDecimal;
import java.util.Currency;

@Converter(autoApply = true)
public class MoneyAttributeConverter implements AttributeConverter<Money, String> {

    @Override
    public String convertToDatabaseColumn(Money money) {
        if (money == null) return null;
        return money.amount().toPlainString() + ":" + money.currency().getCurrencyCode();
    }

    @Override
    public Money convertToEntityAttribute(String dbData) {
        if (dbData == null || dbData.isBlank()) return null;
        String[] parts = dbData.split(":");
        return new Money(new BigDecimal(parts[0]), Currency.getInstance(parts[1]));
    }
}
```

### Example 4: Anti-Pattern (Anemic Entity & Split Repositories) vs DDD Aggregate

#### The Anti-Pattern: Bypassing Aggregate Root & Anemic State
```java
// BAD: Separate repository for an internal child entity
public interface OrderItemRepository extends JpaRepository<OrderItemEntity, Long> {}

@Service
public class BadOrderService {
    @Autowired private OrderItemRepository itemRepo;
    @Autowired private OrderRepository orderRepo;

    public void updateItemQuantity(Long orderId, Long itemId, int newQuantity) {
        // Business invariant violated: Modifying child item directly without
        // recalculating order total or verifying order state!
        OrderItemEntity item = itemRepo.findById(itemId).orElseThrow();
        item.setQuantity(newQuantity);
        itemRepo.save(item);
    }
}
```

#### Production Fix: Aggregate Invariant Protection
```java
// GOOD: Modify internal items ONLY through the Aggregate Root
@Service
@Transactional
public class GoodOrderService {
    private final OrderRepository orderRepository;

    public GoodOrderService(OrderRepository orderRepository) {
        this.orderRepository = orderRepository;
    }

    public void updateItemQuantity(OrderId orderId, OrderItemId itemId, int newQuantity) {
        Order order = orderRepository.findById(orderId)
            .orElseThrow(() -> new OrderNotFoundException(orderId));

        // The aggregate root validates state, recalculates totals, and updates child
        order.changeItemQuantity(itemId, newQuantity);

        orderRepository.save(order); // Entire aggregate persisted together
    }
}
```

### Example 5: Spring Modulith Bounded Context Architecture Test

Verify tactical DDD boundary rules using **Spring Modulith**:

```java
package com.example;

import org.junit.jupiter.api.Test;
import org.springframework.modulith.core.ApplicationModules;
import org.springframework.modulith.docs.Documenter;

class BoundedContextVerificationTest {

    ApplicationModules modules = ApplicationModules.of(Application.class);

    @Test
    void verifyModularBoundaries() {
        // Verifies that domain packages do not leak across bounded contexts
        modules.verify();
    }

    @Test
    void generateC4AndAsciidocDocumentation() {
        // Automatically documents DDD bounded contexts and component diagrams
        new Documenter(modules).writeDocumentation();
    }
}
```

---

## 6. Best Practices

1. **Keep Aggregates Small**: Design aggregates around business invariants, not real-world containment. Prefer referencing other aggregates by Identity (`CustomerId`, `ProductId`) rather than direct JPA object references (`@OneToMany`).
2. **Never Create Repositories for Internal Entities**: Only Aggregate Roots have Repositories. Internal child entities (`OrderItem`) must be saved, updated, and deleted exclusively through the root.
3. **Use Immutable Records for Value Objects**: Java `record`s guarantee structural equality, immutability, and thread safety.
4. **Use `@DomainEvents` for Side Effects**: Decouple domain side effects (notifications, cache evictions, Kafka publishing) by registering events in the aggregate root and processing them via `@TransactionalEventListener`.
5. **Protect Invariants in Constructors and Business Methods**: Eliminate public setters. Validate parameters at the boundary of every business method.
6. **One Aggregate Mutation per Transaction**: In distributed or high-throughput systems, modify only one aggregate root per database transaction. Use domain events and eventual consistency to update secondary aggregates.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| **Giant Aggregates with Lazy Loading** | Connecting everything with `@ManyToMany` / `@OneToMany` leads to massive memory graphs and lock contention. | Reference other aggregates by identifier (e.g., `UUID customerId`) instead of object references. |
| **Creating Repositories for Child Entities** | Callers bypass the aggregate root, violating business rules and corrupting parent state. | Remove `OrderItemRepository`; modify items exclusively via `Order.addItem()` / `Order.removeItem()`. |
| **Using Anemic Setters Everywhere** | Exposes internal state, allowing any class to put the domain model into an invalid state. | Delete all setters. Provide intention-revealing methods (`order.cancel()`, `account.withdraw()`). |
| **Executing Heavy Side Effects Synchronously in Domain** | Sending emails or calling payment APIs inside an entity blocks DB locks and prevents rollback. | Emit a Domain Event and process it in an `AFTER_COMMIT` listener asynchronously. |
| **Confusing Value Objects with Entities** | Giving `Address` or `Money` a synthetic database primary key ID complicates equality checks. | Model Value Objects as Java records and embed them using JPA `@Embeddable` or `AttributeConverter`. |
| **Direct Cross-Context Database Foreign Keys** | Tightly couples different Bounded Contexts, preventing independent database schema migration. | Separate contexts by modular boundaries; communicate via domain events or public API DTOs. |

---

## 8. Performance & Security Notes

### Performance
- **Optimistic Locking**: Always equip Aggregate Roots with `@Version` (JPA) or an integer version column. In high-concurrency environments, optimistic locking prevents lost updates without costly pessimistic database table locks.
- **Cascade Operations**: Configure `cascade = CascadeType.ALL` and `orphanRemoval = true` between the Aggregate Root and internal entities so Spring Data persists the entire aggregate in a single roundtrip.

### Security
- **Self-Validating Aggregates**: Never rely solely on front-end or Web DTO validation. The domain model constructor must reject invalid states (defense-in-depth against API tampering).
- **Tenant Isolation**: Include `TenantId` inside every Aggregate Root in multi-tenant SaaS applications, validating tenant ownership on every repository retrieval.

---

## 9. Comparison / When to Use What

| Architectural Dimension | CRUD-Oriented Architecture | Domain-Driven Design (DDD) |
|---|---|---|
| **Primary Focus** | Database tables & SQL records | Ubiquitous Language & Business Invariants |
| **Entity Model** | Anemic POJOs (Getters/Setters) | Rich Domain Models with Encapsulated Invariants |
| **Relationships** | Deep ORM object graphs (`@ManyToOne`) | Aggregates referencing others by ID |
| **State Changes** | Direct setter mutation by Services | Intent-revealing commands (`order.ship()`) |
| **Integration** | Direct Service-to-Service method calls | Domain Events (`OrderSubmittedEvent`) |
| **Complexity** | Low (Minimal code overhead) | High (Requires strategic and tactical modeling) |
| **Best Used For** | Simple CRUD, prototypes, reporting | Complex business rules, long-lived enterprise core |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>1. What is the difference between an Entity and a Value Object in Domain-Driven Design?</strong></summary>

- **Entity**: Has an explicit, persistent identity (`UUID id`) that remains constant across state changes over time. Two entities with identical attributes but different IDs are distinct instances (e.g., two users named "John Doe").
- **Value Object**: Has no identity; it is defined entirely by the value of its attributes. Value objects are immutable. If any attribute changes, a new value object instance is created. Two value objects with identical attributes are completely interchangeable (e.g., two \$20 dollar bills or identical `Money(20, USD)`).
</details>

<details>
<summary><strong>2. Why should an Aggregate only reference other Aggregates by ID rather than direct object references?</strong></summary>

1. **Transaction Boundaries**: Having direct object references encourages developers to modify multiple aggregates within a single database transaction, causing severe lock contention.
2. **Performance & Memory**: Deep ORM graphs trigger recursive lazy loading or huge join queries, bloating JVM heap memory.
3. **Decoupling for Microservices**: Referencing by ID allows aggregates to be extracted into separate microservices or independent databases with zero refactoring.
</details>

<details>
<summary><strong>3. How does Spring Data's `AbstractAggregateRoot` simplify tactical DDD?</strong></summary>

In pure DDD, an aggregate must publish events when its state changes. Manually injecting `ApplicationEventPublisher` into an entity violates domain purity.
Spring Data provides `AbstractAggregateRoot`:
- The entity calls `registerEvent(event)` internally.
- The events are stored in a transient list inside the entity.
- When `repository.save(entity)` is called, Spring Data automatically retrieves the events, publishes them to the Spring context, and clears the internal list.
</details>

<details>
<summary><strong>4. What is the purpose of `@TransactionalEventListener` with `phase = AFTER_COMMIT`?</strong></summary>

Standard Spring `@EventListener` executes synchronously within the active transaction. If an event listener sends an email or calls Stripe, and the database transaction subsequently rolls back due to a constraint violation, an email was sent for an order that never existed!
`@TransactionalEventListener(phase = TransactionPhase.AFTER_COMMIT)` guarantees that the event handler executes *only after* the database transaction commits successfully, ensuring consistency between database state and external side effects.
</details>

<details>
<summary><strong>5. What is a Bounded Context and how is it realized in a Spring Boot application?</strong></summary>

A Bounded Context is an explicit boundary within which a domain model applies and terms in the Ubiquitous Language have a specific, unambiguous meaning.
For example, `Product` in the *Catalog Context* represents marketing descriptions, categories, and images. In the *Shipping Context*, `Product` represents dimensions, weight, and hazardous material codes.
In Spring Boot:
- Realized as separate microservices, OR
- Realized as isolated top-level packages verified by **Spring Modulith** in a modular monolith.
</details>

<details>
<summary><strong>6. How should optimistic locking be implemented on an Aggregate Root?</strong></summary>

By adding a `@Version` field (e.g., `Long version`) on the Aggregate Root.
When two concurrent requests attempt to modify the same aggregate (e.g., adding items simultaneously):
- The first transaction commits and increments the version from 1 to 2.
- The second transaction detects that the expected version in the database is no longer 1 and fails immediately with `OptimisticLockingFailureException`.
This guarantees aggregate invariant consistency without pessimistic database row locking.
</details>

<details>
<summary><strong>7. When should you introduce a Domain Service instead of adding methods to an Entity?</strong></summary>

Use a Domain Service when:
1. The operation represents a significant business concept that involves multiple distinct Aggregate Roots (e.g., transferring funds between two `Account` aggregates).
2. The calculation requires external domain inputs that would pollute an entity (e.g., `TaxCalculatorService` requiring fluctuating government tax tables).
3. The operation does not naturally belong to any single entity.
</details>

<details>
<summary><strong>8. How does jMolecules help enforce DDD conventions in a Spring Boot codebase?</strong></summary>

**jMolecules** provides standard DDD annotations (`@AggregateRoot`, `@Entity`, `@ValueObject`, `@Repository`) and ArchUnit verification rules.
It allows developers to annotate classes with architectural intent and run automated static analysis to ensure that:
- Aggregates do not directly reference other aggregates.
- Value objects are immutable.
- Repositories only exist for Aggregate Roots.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Create a Strongly-Typed Identifier Value Object</strong></summary>

**Objective**: Implement a type-safe `CustomerId` record that prevents passing raw `UUID`s or strings accidentally.

**Solution**:
```java
public record CustomerId(UUID value) {
    public CustomerId {
        Objects.requireNonNull(value, "CustomerId value cannot be null");
    }

    public static CustomerId generate() {
        return new CustomerId(UUID.randomUUID());
    }

    public static CustomerId fromString(String id) {
        return new CustomerId(UUID.fromString(id));
    }
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Implement an Invariant Guard on Account Withdrawal</strong></summary>

**Objective**: Protect bank account invariants against overdraft and negative withdrawals.

**Solution**:
```java
public class BankAccount {
    private final AccountId id;
    private Money balance;

    public BankAccount(AccountId id, Money initialDeposit) {
        this.id = Objects.requireNonNull(id);
        this.balance = Objects.requireNonNull(initialDeposit);
    }

    public void withdraw(Money amount) {
        Objects.requireNonNull(amount);
        if (amount.amount().compareTo(BigDecimal.ZERO) <= 0) {
            throw new IllegalArgumentException("Withdrawal amount must be positive");
        }
        if (this.balance.amount().compareTo(amount.amount()) < 0) {
            throw new InsufficientFundsException(id, amount, balance);
        }
        this.balance = new Money(this.balance.amount().subtract(amount.amount()), this.balance.currency());
    }

    public Money getBalance() { return balance; }
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Implement Domain Event Registration with Spring Data</strong></summary>

**Objective**: Create an `Order` aggregate extending `AbstractAggregateRoot` that emits an `OrderCancelledEvent`.

**Solution**:
```java
public class Order extends AbstractAggregateRoot<Order> {
    private final OrderId id;
    private OrderStatus status = OrderStatus.PLACED;

    public Order(OrderId id) { this.id = id; }

    public void cancel(String reason) {
        if (this.status == OrderStatus.SHIPPED) {
            throw new IllegalStateException("Cannot cancel an already shipped order");
        }
        this.status = OrderStatus.CANCELLED;
        // Register event for automatic publication upon repository.save()
        registerEvent(new OrderCancelledEvent(this.id, reason, Instant.now()));
    }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Map a DDD Value Object to Multiple JPA Columns using `@Embeddable`</strong></summary>

**Objective**: Embed an `Address` Value Object into a `CustomerEntity` using JPA `@Embedded`.

**Solution**:
```java
@Embeddable
public record Address(String street, String city, String zipCode, String country) {
    protected Address() { this(null, null, null, null); } // JPA requirement
}

@Entity
@Table(name = "customers")
public class CustomerEntity {
    @Id
    private UUID id;

    @Embedded
    @AttributeOverrides({
        @AttributeOverride(name = "street", column = @Column(name = "addr_street")),
        @AttributeOverride(name = "city", column = @Column(name = "addr_city")),
        @AttributeOverride(name = "zipCode", column = @Column(name = "addr_zip")),
        @AttributeOverride(name = "country", column = @Column(name = "addr_country"))
    })
    private Address shippingAddress;
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Implement an Outbox Table Pattern with Spring Modulith Events</strong></summary>

**Objective**: Use Spring Modulith's built-in Event Publication Registry to guarantee at-least-once asynchronous domain event delivery across database boundaries.

**Solution**:
1. Add dependency: `org.springframework.modulith:spring-modulith-starter-jpa`.
2. Configure listener:
```java
@Component
public class KafkaOrderEventRelay {

    private final KafkaTemplate<String, Object> kafkaTemplate;

    public KafkaOrderEventRelay(KafkaTemplate<String, Object> kafkaTemplate) {
        this.kafkaTemplate = kafkaTemplate;
    }

    @ApplicationModuleListener
    public void onOrderSubmitted(OrderSubmittedEvent event) {
        // Automatically persisted to event_publication table within the local DB transaction!
        // If Kafka publishing fails, Spring Modulith automatically retries on startup/timer.
        kafkaTemplate.send("orders-topic", event.orderId().toString(), event);
    }
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Build an ArchUnit Rule Verifying Aggregate Root Encapsulation</strong></summary>

**Objective**: Write an ArchUnit test asserting that repositories are only declared for types annotated with `@AggregateRoot`.

**Solution**:
```java
@AnalyzeClasses(packages = "com.example")
public class AggregateRepositoryRuleTest {

    @ArchTest
    public static final ArchRule repositories_must_only_manage_aggregate_roots = 
        classes().that().areAssignableTo(Repository.class)
            .should(new ArchCondition<JavaClass>("only manage @AggregateRoot entities") {
                @Override
                public void check(JavaClass repositoryClass, ConditionEvents events) {
                    // Extract domain generic type and assert it has @AggregateRoot
                }
            });
}
```
</details>

---

## 12. Summary & Cheat Sheet

```
+----------------------------------------------------------------------------------------------------+
| DOMAIN-DRIVEN DESIGN (DDD) QUICK REFERENCE                                                         |
+----------------------------------------------------------------------------------------------------+
| Tactical Element    | Java / Spring Implementation              | Core Rule                        |
+---------------------+-------------------------------------------+----------------------------------+
| Aggregate Root      | Class extending AbstractAggregateRoot     | Only entry point for changes;    |
|                     | with @Version for optimistic locking      | enforces all invariants          |
+---------------------+-------------------------------------------+----------------------------------+
| Value Object        | Java 21 record; @Embeddable               | Immutable; no identity; equal by |
|                     | or AttributeConverter                     | attributes; self-validating      |
+---------------------+-------------------------------------------+----------------------------------+
| Internal Entity     | Class without independent Repository      | Managed entirely through Root    |
+---------------------+-------------------------------------------+----------------------------------+
| Domain Event        | Java record; registerEvent() in root      | Record of past domain occurrence |
+---------------------+-------------------------------------------+----------------------------------+
| Event Listener      | @TransactionalEventListener(AFTER_COMMIT) | Executes after DB commit succeeds|
+---------------------+-------------------------------------------+----------------------------------+
| Repository          | Spring Data JpaRepository for Root ONLY   | Collection-like aggregate store  |
+----------------------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Domain-Driven Design: Tackling Complexity in the Heart of Software (Eric Evans)](https://www.domainlanguage.com/ddd/)
- [Implementing Domain-Driven Design (Vaughn Vernon)](https://kalele.io/iddd/)
- [Spring Modulith Documentation](https://docs.spring.io/spring-modulith/reference/)
- [jMolecules: Architectural Abstractions for Java](https://github.com/xmolecules/jmolecules)
- [Martin Fowler on Anemic Domain Model](https://martinfowler.com/bliki/AnemicDomainModel.html)
