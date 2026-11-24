# Application Events & Listeners: Synchronous, Async & Transactional

> The definitive guide to Spring's internal event-driven architecture: domain events with Java 21 records, `@EventListener`, conditional filtering, asynchronous processing with `@Async`, transaction-bound execution via `@TransactionalEventListener`, and Spring Modulith event outbox patterns.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [IoC Container & ApplicationContext Internals](file:///03-backend/spring-boot/01-core-spring/01-ioc-container-and-application-context.md), [JavaConfig, @Configuration & @Bean Internals](file:///03-backend/spring-boot/01-core-spring/05-javaconfig-and-at-configuration.md) |
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
In monolithic and modular systems, executing cross-cutting post-actions (sending emails, updating read models, triggering audit logs, clearing caches) directly inside a core business service tightly couples modules. If the `OrderService` directly calls `EmailService`, `InventoryService`, `AnalyticsService`, and `BillingService`, the class violates the Single Responsibility Principle, and a failure in an auxiliary task (like an email timeout) can roll back an entire database transaction.

### Historical Evolution
- **Spring 1.x - 4.1**: Events had to extend `ApplicationEvent` and listeners had to implement `ApplicationListener<E>`, creating heavyweight framework coupling.
- **Spring 4.2**: Introduced annotation-driven `@EventListener` and payload events (any POJO/Record can be an event).
- **Spring 4.2+ & Boot 3.x**: Added `@TransactionalEventListener` to tie event handling to transaction completion phases (`AFTER_COMMIT`), eliminating ghost notifications on transaction rollback.

### Real-World Relevance
Spring application events provide in-process decoupling. When paired with Java 21 records and `@TransactionalEventListener`, they form the architectural backbone for Domain-Driven Design (DDD) aggregates and Modular Monoliths.

---

## 2. Core Concepts

### 1. The Core Event Model
- **Event Object**: In modern Spring, any Java object can be an event. Best practice is to use **Java 21 `record`** for immutable domain events.
- **Publisher**: Components inject `ApplicationEventPublisher` and invoke `publishEvent(Object event)`.
- **Listener**: Any bean method annotated with `@EventListener`.

### 2. Synchronous vs. Asynchronous Events
- **Default (Synchronous)**: Listeners execute sequentially on the publisher's thread within the active transaction boundary. If a listener throws an unhandled exception, the caller fails.
- **Asynchronous (`@Async`)**: When combined with `@EnableAsync` and `@Async`, listeners execute in a separate worker thread (or virtual thread), freeing the caller immediately.

### 3. Transaction-Bound Events (`@TransactionalEventListener`)
Allows listeners to execute only during specific phases of the enclosing database transaction:
- **`AFTER_COMMIT` (Default)**: Executes only after the database transaction successfully commits. Crucial for sending emails or publishing external Kafka messages.
- **`AFTER_ROLLBACK`**: Executes only if the transaction rolls back (e.g., compensation logic, rollback alerts).
- **`BEFORE_COMMIT`**: Executes right before commit (e.g., updating audit tables within the same transaction).
- **`AFTER_COMPLETION`**: Executes on both commit and rollback.

---

## 3. How It Works Internally

### Event Dispatch and Transaction Synchronization Architecture

```
+-------------------------------------------------------------------------+
|                  OrderService (@Transactional)                          |
|  - Inserts order record into PostgreSQL                                 |
|  - `publisher.publishEvent(new OrderPlacedEvent(orderId, amount))`       |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  ApplicationEventMulticaster                            |
|  - Resolves registered listeners matching `OrderPlacedEvent`            |
+------------------------------------+------------------------------------+
                                     |
             +-----------------------+-----------------------+
             |                                               |
             v                                               v
+-----------------------------+     +-------------------------------------+
| Standard @EventListener     |     | @TransactionalEventListener         |
| - Executes synchronously    |     |   (phase = TransactionPhase.        |
|   on caller thread          |     |              AFTER_COMMIT)          |
| - Shares DB transaction     |     | - Registers callback with Spring's  |
|                             |     |   `TransactionSynchronizationManager`|
+-----------------------------+     +------------------+------------------+
                                                       |
                                        PostgreSQL COMMIT Successfully Completed
                                                       |
                                                       v
                                    +-------------------------------------+
                                    | Executes AFTER_COMMIT Listener:     |
                                    | - Dispatches Welcome Email          |
                                    | - Publishes to External Kafka Broker|
                                    +-------------------------------------+
```

---

## 4. Syntax & Usage

### Basic Event Definition & Publication
```java
// Immutable Domain Event using Java 21 Record
public record OrderPlacedEvent(UUID orderId, String customerEmail, double amount) {}

@Service
public class OrderService {
    private final ApplicationEventPublisher publisher;

    public OrderService(ApplicationEventPublisher publisher) {
        this.publisher = publisher;
    }

    public void checkout(UUID id, String email, double total) {
        // Business logic...
        publisher.publishEvent(new OrderPlacedEvent(id, email, total));
    }
}
```

### Conditional Listener with SpEL
```java
@Component
public class VipOrderListener {

    // Listens ONLY if order amount exceeds $1,000
    @EventListener(condition = "#event.amount() > 1000")
    public void handleHighValueOrder(OrderPlacedEvent event) {
        System.out.println("High-value VIP order placed: " + event.orderId());
    }
}
```

---

## 5. Practical Examples

### Example 1: Domain-Driven Event Publication with Java 21 Records
```java
package com.example.demo.events;

import org.springframework.context.ApplicationEventPublisher;
import org.springframework.context.event.EventListener;
import org.springframework.stereotype.Component;
import org.springframework.stereotype.Service;

import java.time.Instant;
import java.util.UUID;

// Immutable Event Record
public record UserRegisteredEvent(UUID userId, String username, String email, Instant registeredAt) {}

@Service
public class UserService {

    private final ApplicationEventPublisher publisher;

    public UserService(ApplicationEventPublisher publisher) {
        this.publisher = publisher;
    }

    public void registerUser(String username, String email) {
        UUID newUserId = UUID.randomUUID();
        System.out.println("User registered in database: " + newUserId);

        // Decoupled notification via event
        publisher.publishEvent(new UserRegisteredEvent(newUserId, username, email, Instant.now()));
    }
}

@Component
class WelcomeEmailListener {

    @EventListener
    public void onUserRegistered(UserRegisteredEvent event) {
        System.out.printf("[EMAIL DISPATCH] Sent welcome pack to %s (%s)%n", 
            event.username(), event.email());
    }
}
```

### Example 2: `@TransactionalEventListener` with `AFTER_COMMIT`
```java
package com.example.demo.transactional;

import org.springframework.stereotype.Component;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;
import org.springframework.transaction.event.TransactionPhase;
import org.springframework.transaction.event.TransactionalEventListener;

import java.util.UUID;

public record PaymentCompletedEvent(UUID transactionId, double amount) {}

@Service
public class PaymentProcessingService {

    private final org.springframework.context.ApplicationEventPublisher publisher;

    public PaymentProcessingService(org.springframework.context.ApplicationEventPublisher publisher) {
        this.publisher = publisher;
    }

    @Transactional
    public void processPayment(UUID txnId, double amount) {
        System.out.println("Payment recorded in DB ledger: " + txnId);
        // Event published inside transaction
        publisher.publishEvent(new PaymentCompletedEvent(txnId, amount));
        // If an exception occurs here, the transaction rolls back, 
        // and the AFTER_COMMIT listener below will NEVER fire!
    }
}

@Component
class PaymentAuditEmitter {

    // Executes ONLY if the database transaction successfully commits
    @TransactionalEventListener(phase = TransactionPhase.AFTER_COMMIT)
    public void dispatchExternalWebhook(PaymentCompletedEvent event) {
        System.out.println("External Webhook dispatched for committed transaction: " + event.transactionId());
    }

    // Executes ONLY if the database transaction rolls back
    @TransactionalEventListener(phase = TransactionPhase.AFTER_ROLLBACK)
    public void onRollback(PaymentCompletedEvent event) {
        System.err.println("ALERT: Payment transaction rolled back for: " + event.transactionId());
    }
}
```

### Example 3: Asynchronous Event Processing with Virtual Threads
```java
package com.example.demo.async;

import org.springframework.context.annotation.Configuration;
import org.springframework.context.event.EventListener;
import org.springframework.scheduling.annotation.Async;
import org.springframework.scheduling.annotation.EnableAsync;
import org.springframework.stereotype.Component;

import java.util.UUID;

public record LargeReportRequestedEvent(UUID reportId) {}

@Configuration
@EnableAsync
public class AsyncConfig {}

@Component
public class AsyncReportListener {

    // Offloads execution to background thread, unblocking caller immediately
    @Async
    @EventListener
    public void generateReportAsync(LargeReportRequestedEvent event) {
        System.out.printf("Async report generation started on thread: %s%n", 
            Thread.currentThread().toString());
        try {
            Thread.sleep(3000); // Heavy computation
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
        System.out.println("Async report completed: " + event.reportId());
    }
}
```

### Example 4: The Read-Only Connection Anti-Pattern in `AFTER_COMMIT` & Remediation
```java
// ANTI-PATTERN: Attempting to modify the database inside AFTER_COMMIT without REQUIRES_NEW
@Component
public class BrokenAfterCommitListener {

    @Autowired
    private AuditLogRepository auditRepository;

    @TransactionalEventListener(phase = TransactionPhase.AFTER_COMMIT)
    public void onOrderPlaced(OrderPlacedEvent event) {
        // FATAL FLAW: The original transaction has ALREADY COMMITTED!
        // Reusing the same connection for writing throws:
        // "Transaction is already completed - do not call commit or rollback"
        // or silently ignores database updates!
        auditRepository.save(new AuditLog(event.orderId()));
    }
}

// REMEDIATION: Mark listener with @Transactional(propagation = Propagation.REQUIRES_NEW)
@Component
public class CleanAfterCommitListener {

    private final AuditLogRepository auditRepository;

    public CleanAfterCommitListener(AuditLogRepository auditRepository) {
        this.auditRepository = auditRepository;
    }

    // Opens a BRAND NEW, independent database transaction for the audit log
    @TransactionalEventListener(phase = TransactionPhase.AFTER_COMMIT)
    @org.springframework.transaction.annotation.Transactional(
        propagation = org.springframework.transaction.annotation.Propagation.REQUIRES_NEW
    )
    public void onOrderPlaced(OrderPlacedEvent event) {
        auditRepository.save(new AuditLog(event.orderId()));
    }
}
```

### Example 5: Generic Events with Parameterized Type Matching
```java
package com.example.demo.generics;

import org.springframework.context.event.EventListener;
import org.springframework.stereotype.Component;

// Generic Event Wrapper
public record EntityCreatedEvent<T>(T entity, String createdBy) {}

public record Customer(String id, String name) {}
public record Invoice(String id, double total) {}

@Component
public class GenericEventListener {

    // Invocations are filtered automatically by generic type argument!
    @EventListener
    public void onCustomerCreated(EntityCreatedEvent<Customer> event) {
        System.out.println("New Customer Created: " + event.entity().name());
    }

    @EventListener
    public void onInvoiceCreated(EntityCreatedEvent<Invoice> event) {
        System.out.println("New Invoice Created: " + event.entity().id());
    }
}
```

---

## 6. Best Practices

1. **Model Events with Java Records**: Use Java 21 `record` for all event payloads. Records guarantee immutability, thread safety, and concise syntax.
2. **Use `@TransactionalEventListener(AFTER_COMMIT)` for Side-Effects**: External network calls (sending SMS, publishing to Kafka, calling Stripe) must run *after* the local database transaction commits to prevent ghost actions on rollback.
3. **Always Use `Propagation.REQUIRES_NEW` If Modifying DB in `AFTER_COMMIT`**: Because the original transaction has completed, any subsequent database writes in an `AFTER_COMMIT` listener require a new transaction.
4. **Use Virtual Threads for `@Async` Listeners**: Enable `spring.threads.virtual.enabled=true` so `@Async` event listeners spawn lightweight virtual threads instead of saturating platform thread pools.
5. **Keep Listeners Idempotent**: External systems can retry or deliver duplicate messages. Ensure event listeners can handle the same event twice safely.
6. **Order Listeners Explicitly**: When multiple listeners consume the same event, annotate them with `@Order(int)` to ensure deterministic execution sequence.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Writing to DB in `AFTER_COMMIT` without `REQUIRES_NEW`** | The connection is already closed/committed; modifications fail silently or throw exceptions. | Annotate listener with `@Transactional(propagation = Propagation.REQUIRES_NEW)`. |
| **Publishing external events in standard `@EventListener`** | If database transaction rolls back, external side effects (like emails) have already been sent! | Use `@TransactionalEventListener(phase = TransactionPhase.AFTER_COMMIT)`. |
| **Forgetting `@EnableAsync`** | Adding `@Async` on an event listener without `@EnableAsync` in configuration runs the listener synchronously on the caller's thread. | Add `@EnableAsync` to a `@Configuration` class. |
| **Unhandled exception in synchronous listener** | Unchecked exceptions in standard `@EventListener` bubble up to the publisher, causing the entire caller operation to fail. | Wrap listener logic in `try-catch` or make the listener `@Async`. |
| **Publishing mutable event objects** | Modifying an event object in Listener A corrupts the payload for Listener B. | Always use immutable Java records for event classes. |
| **No transaction active for `@TransactionalEventListener`** | If published outside an active transaction, `@TransactionalEventListener` does NOT fire by default. | Set `fallbackExecution = true` if the listener should also run non-transactionally. |

---

## 8. Performance & Security Notes

### Synchronous Listener Latency
Standard `@EventListener` executes sequentially on the caller's thread. If three listeners each take 50ms, the HTTP request latency increases by 150ms:
- Move non-critical listeners to `@Async` or dispatch via background queues.

### Security Context Propagation
When using `@Async` event listeners, the Spring Security `SecurityContext` (holding authenticated user details) is **not** propagated to the new thread by default. Configure `SecurityContextHolder.setStrategyName(SecurityContextHolder.MODE_INHERITABLETHREADLOCAL)` or pass user IDs explicitly inside the event record.

---

## 9. Comparison / When to Use What

| Event Mechanism | Scope | Durability | Transaction Bound? | Best For |
| :--- | :--- | :--- | :--- | :--- |
| **Spring `@EventListener`** | In-Process (Single JVM) | In-Memory (Ephemeral) | No (Synchronous) | Decoupling in-memory components |
| **`@TransactionalEventListener`** | In-Process (Single JVM) | In-Memory (Ephemeral) | **Yes (`AFTER_COMMIT`)** | Audit logs, post-commit dispatch |
| **Spring Modulith Outbox** | In-Process to External | **Persistent (DB Outbox)**| **Yes (Atomic with DB)** | Guaranteed event delivery across modules |
| **Apache Kafka / RabbitMQ** | Distributed System | Persistent (Disk/Cluster) | Requires Outbox Pattern | Microservice-to-microservice async events |

---

## 10. Interview Questions

<details>
<summary><strong>Q1: What is the difference between `@EventListener` and `@TransactionalEventListener`?</strong></summary>

**Answer:**  
- `@EventListener`: Executes immediately and synchronously on the publishing thread, participating in the caller's active transaction (if any).
- `@TransactionalEventListener`: An extension of `@EventListener` that binds listener execution to a specific phase of the active transaction (default: `TransactionPhase.AFTER_COMMIT`). It executes only after the transaction successfully completes, preventing side-effects on rollback.
</details>

<details>
<summary><strong>Q2: What happens if an event is published using `@TransactionalEventListener` but no database transaction is active?</strong></summary>

**Answer:**  
By default, the listener will **not execute at all**, and the event is silently ignored. If you want the listener to execute even when no transaction exists, set the attribute `fallbackExecution = true`:
```java
@TransactionalEventListener(fallbackExecution = true)
```
</details>

<details>
<summary><strong>Q3: Why must you use `Propagation.REQUIRES_NEW` when saving entities inside an `AFTER_COMMIT` listener?</strong></summary>

**Answer:**  
In the `AFTER_COMMIT` phase, the caller's transaction has already completed and committed, and its underlying database connection is closed or returned to the pool. Attempting to execute DML operations on that completed transaction context will fail or produce no effect. Adding `@Transactional(propagation = Propagation.REQUIRES_NEW)` forces Spring to open a brand-new, independent database transaction with a fresh connection.
</details>

<details>
<summary><strong>Q4: How do you make an event listener asynchronous in Spring?</strong></summary>

**Answer:**  
1. Add `@EnableAsync` to a `@Configuration` class.
2. Annotate the listener method with both `@Async` and `@EventListener`.
The listener will be offloaded to a `TaskExecutor` thread pool (or virtual thread), executing in the background without blocking the publisher.
</details>

<details>
<summary><strong>Q5: What are the four transaction phases supported by `@TransactionalEventListener`?</strong></summary>

**Answer:**  
1. `AFTER_COMMIT` (default): Triggered after successful transaction commit.
2. `AFTER_ROLLBACK`: Triggered if the transaction rolls back.
3. `BEFORE_COMMIT`: Triggered right before the transaction commits.
4. `AFTER_COMPLETION`: Triggered on both commit and rollback.
</details>

<details>
<summary><strong>Q6: How can you conditionally filter events in an `@EventListener` using SpEL?</strong></summary>

**Answer:**  
Use the `condition` attribute with Spring Expression Language (SpEL):
```java
@EventListener(condition = "#event.total > 500 and #event.currency == 'USD'")
public void handleHighValueUsdOrder(OrderPlacedEvent event) { ... }
```
The listener will only be triggered if the boolean SpEL expression evaluates to `true`.
</details>

<details>
<summary><strong>Q7: How does Spring resolve generic event types (e.g., `EntityCreatedEvent<Order>`)?</strong></summary>

**Answer:**  
Spring's `ApplicationEventMulticaster` inspects the generic type parameters via reflection (`ResolvableType`). If a listener declares `onEvent(EntityCreatedEvent<Order> event)`, Spring will only route events where the generic parameter matches `Order`, filtering out `EntityCreatedEvent<Customer>`.
</details>

<details>
<summary><strong>Q8: What is the benefit of using Java 21 Records for Spring events?</strong></summary>

**Answer:**  
Records provide:
1. **Immutability**: All fields are `final`, preventing listeners from accidentally mutating event state and corrupting other listeners.
2. **Conciseness**: Automatically generates constructor, getters, `equals()`, `hashCode()`, and `toString()`.
3. **Thread Safety**: Safe to publish across concurrent `@Async` threads without defensive copying.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Create a Domain Record Event (Easy)</strong></summary>

**Task:**  
Create an immutable `InvoiceGeneratedEvent(UUID id, double amount)` record. Publish it from an `InvoiceService` and consume it in a `@Component` listener that logs the invoice details.
</details>

<details>
<summary><strong>Exercise 2: Conditional SpEL Event Filtering (Easy)</strong></summary>

**Task:**  
Add a condition to the listener from Exercise 1 so that it only executes if the amount is greater than 100.0 (`#event.amount() > 100.0`). Publish events with 50.0 and 150.0 and verify only 150.0 is logged.
</details>

<details>
<summary><strong>Exercise 3: Test `AFTER_ROLLBACK` Handling (Medium)</strong></summary>

**Task:**  
Write a `@Transactional` service method that publishes a `StockReservationEvent` and then intentionally throws a `RuntimeException`. Verify that an `@TransactionalEventListener(phase = TransactionPhase.AFTER_ROLLBACK)` executes while the `AFTER_COMMIT` listener does not.
</details>

<details>
<summary><strong>Exercise 4: Asynchronous Virtual Thread Event Listener (Medium)</strong></summary>

**Task:**  
Enable virtual threads in Spring Boot 3.4. Annotate an event listener with `@Async`. Publish an event and verify that `Thread.currentThread().isVirtual()` returns `true` inside the listener method.
</details>

<details>
<summary><strong>Exercise 5: Fix Read-Only DB in `AFTER_COMMIT` (Hard)</strong></summary>

**Task:**  
Write a test reproducing the failed database write in an `AFTER_COMMIT` listener. Annotate the listener with `@Transactional(propagation = Propagation.REQUIRES_NEW)` and assert that the entity is successfully persisted in the database after the original transaction completes.
</details>

<details>
<summary><strong>Exercise 6: Generic Type Event Routing (Hard)</strong></summary>

**Task:**  
Create a generic class `AuditEvent<T>(T payload, Instant timestamp)`. Implement two separate listeners: one for `AuditEvent<Order>` and one for `AuditEvent<User>`. Publish both types and assert that each listener receives strictly its own type.
</details>

---

## 12. Summary / Cheat Sheet

| Annotation / Class | Attribute / Syntax | Execution Characteristics |
| :--- | :--- | :--- |
| **`@EventListener`** | `@EventListener` | Synchronous, caller thread, participates in TX |
| **SpEL Condition** | `@EventListener(condition = "#e.active")`| Conditional execution based on boolean SpEL |
| **`@Async`** | `@Async @EventListener` | Asynchronous worker/virtual thread execution |
| **`AFTER_COMMIT`** | `@TransactionalEventListener` | Executes only after DB transaction commits |
| **`AFTER_ROLLBACK`** | `phase = TransactionPhase.AFTER_ROLLBACK`| Executes only on transaction rollback |
| **New Transaction** | `@Transactional(propagation = REQUIRES_NEW)`| Mandatory when persisting data in `AFTER_COMMIT` |
| **Generic Events** | `@EventListener void on(Event<User> e)` | Automatically filtered by generic type parameter |

---

## 13. Further Reading
- [Official Spring Framework Reference: Standard and Custom Events](https://docs.spring.io/spring-framework/reference/core/beans/context-introduction.html#context-functionality-events)
- [Spring Framework Transactional Event Listeners](https://docs.spring.io/spring-framework/reference/data-access/transaction/event.html)
- [Spring Modulith Event Publication Registry](https://docs.spring.io/spring-modulith/reference/events.html)
