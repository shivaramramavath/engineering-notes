# Spring ApplicationEvents and Transaction-Bound Events

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | `ApplicationEventPublisher`, `@EventListener`, `@TransactionalEventListener` |

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
When building domain-driven systems, an entity state mutation (such as an order being paid or an account registered) frequently requires secondary actions: dispatching notification emails, updating analytical caches, and synchronizing search indexes. Directly coupling these side-effects inside the primary business service bloats code, introduces unwanted dependencies, complicates testing, and creates transactional hazards. For example, if a service dispatches an email before committing the database transaction, and the database subsequently rolls back due to a constraint violation, the user receives an email confirming an order that does not exist in the database.

### The Evolution
Spring Framework provides an in-process Observer pattern implementation via `ApplicationEventPublisher` and `@EventListener`. Spring Framework 4.2 introduced `@TransactionalEventListener`, allowing listeners to bind execution strictly to specific database transaction lifecycle phases (`BEFORE_COMMIT`, `AFTER_COMMIT`, `AFTER_ROLLBACK`, `AFTER_COMPLETION`). In Spring Boot 3, events can be plain Java records without extending any framework base classes, enabling decoupled domain architectures.

### Relevance in Modern Engineering
In monolithic modularization (Modular Monoliths) and Domain-Driven Design (DDD), in-process domain events allow modules to communicate asynchronously without hard direct method references, keeping module boundaries clean while guaranteeing transactional consistency.

---

## 2. Core Concepts

- **`ApplicationEventPublisher`**: The Spring interface used to publish arbitrary POJO or record events into the application context.
- **`@EventListener`**: Standard listener annotation that invokes the annotated method synchronously (or asynchronously with `@Async`) on the publisher's thread.
- **`@TransactionalEventListener`**: Specialized listener that synchronizes with the surrounding database transaction (`PlatformTransactionManager`), executing only when the active transaction reaches the specified phase.
- **Transaction Phases (`TransactionPhase`)**:
  - `AFTER_COMMIT` (Default): Executes only after the transaction has successfully committed to the database.
  - `BEFORE_COMMIT`: Executes immediately before the database commit phase.
  - `AFTER_ROLLBACK`: Executes only if the transaction rolls back.
  - `AFTER_COMPLETION`: Executes after the transaction finishes (whether committed or rolled back).
- **`fallbackExecution`**: Attribute determining whether the listener executes if no active transaction is running when the event is published (defaults to `false`).

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                  Transactional Event Lifecycle Architecture                       |
|                                                                                   |
| 1. Service Layer Begins Transaction                                               |
|    @Transactional                                                                 |
|    public void placeOrder(...) {                                                  |
|        orderRepository.save(order);                                               |
|        publisher.publishEvent(new OrderPlacedEvent(order.getId()));               |
|    }                                                                              |
|                   |                                                               |
|                   v                                                               |
| 2. Event Published During Active Transaction                                      |
|    +-----------------------------+                                                |
|    | ApplicationEventMulticaster | ---> Detects @TransactionalEventListener       |
|    +-----------------------------+                                                |
|                   |                                                               |
|                   v                                                               |
| 3. Registration in TransactionSynchronizationManager                              |
|    TransactionSynchronizationManager.registerSynchronization(new SyncCallback()) |
|    (Method execution is PAUSED and held in thread-local transaction registry)     |
|                   |                                                               |
|                   v                                                               |
| 4. Database Transaction Commits                                                   |
|    Connection.commit() ---> Success!                                              |
|                   |                                                               |
|                   v                                                               |
| 5. TransactionSynchronization.afterCommit() Triggered                             |
|    +---------------------------------------------------------------------+        |
|    | Listener Method Invoked: onOrderPlaced(OrderPlacedEvent event)      |        |
|    | (Now safe to dispatch emails, push Kafka messages, notify clients)  |        |
|    +---------------------------------------------------------------------+        |
+-----------------------------------------------------------------------------------+
```

When `publishEvent()` is called, `ApplicationEventMulticaster` inspects the listener method. Discovering `@TransactionalEventListener`, it checks `TransactionSynchronizationManager.isActualTransactionActive()`. If an active transaction exists, it defers method execution by registering a `TransactionSynchronization` callback. When the `PlatformTransactionManager` issues `Connection.commit()`, it triggers `afterCommit()` on all registered synchronizations, executing the listener method.

---

## 4. Syntax & Usage

### Basic Event Publisher and Listener
```java
// Immutable Domain Event Record
public record OrderPlacedEvent(String orderId, double totalAmount) {}

// Publisher
@Service
public class OrderService {
    private final ApplicationEventPublisher eventPublisher;

    public OrderService(ApplicationEventPublisher eventPublisher) {
        this.eventPublisher = eventPublisher;
    }

    @Transactional
    public void createOrder(String orderId, double amount) {
        // DB write operations...
        eventPublisher.publishEvent(new OrderPlacedEvent(orderId, amount));
    }
}

// Transaction-Bound Listener
@Component
public class OrderNotificationListener {

    @TransactionalEventListener(phase = TransactionPhase.AFTER_COMMIT)
    public void onOrderPlaced(OrderPlacedEvent event) {
        // Executes strictly after DB commit succeeds
    }
}
```

---

## 5. Practical Examples

### Example 1: Domain Event with Asynchronous Post-Commit Processing
```java
package com.example.platform.events.listener;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.scheduling.annotation.Async;
import org.springframework.stereotype.Component;
import org.springframework.transaction.event.TransactionPhase;
import org.springframework.transaction.event.TransactionalEventListener;

public record UserRegisteredEvent(String userId, String email, String fullName) {}

@Component
public class WelcomeEmailEventListener {

    private static final Logger log = LoggerFactory.getLogger(WelcomeEmailEventListener.class);

    // Combining @Async and @TransactionalEventListener ensures:
    // 1. Executes strictly AFTER DB commit
    // 2. Offloads execution to background thread pool to prevent blocking user HTTP request
    @Async("platformTaskExecutor")
    @TransactionalEventListener(phase = TransactionPhase.AFTER_COMMIT)
    public void handleUserRegistration(UserRegisteredEvent event) {
        log.info("Sending welcome email to {} ({}) on thread: {}",
                event.email(), event.userId(), Thread.currentThread().getName());
        // Call email provider (SES / SendGrid)
    }
}
```

### Example 2: Compensating Transaction on Rollback (`AFTER_ROLLBACK`)
```java
package com.example.platform.events.listener;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Component;
import org.springframework.transaction.event.TransactionPhase;
import org.springframework.transaction.event.TransactionalEventListener;

public record InventoryReservationFailedEvent(String reservationId, String sku, int quantity) {}

@Component
public class InventoryRollbackListener {

    private static final Logger log = LoggerFactory.getLogger(InventoryRollbackListener.class);

    @TransactionalEventListener(phase = TransactionPhase.AFTER_ROLLBACK)
    public void onReservationFailed(InventoryReservationFailedEvent event) {
        log.warn("Database transaction rolled back. Triggering compensation for reservation: {}",
                event.reservationId());
        // Clean up external reservation locks in Redis or third-party warehouse
    }
}
```

### Example 3: Modifying Database Inside `AFTER_COMMIT` (Anti-Pattern Fix)
```java
// ANTI-PATTERN: Calling a repository.save() inside an AFTER_COMMIT listener without starting a new transaction.
// Because the original transaction has ALREADY committed, the current connection is closed or in auto-commit,
// causing Hibernate flush exceptions or silent omission of writes!

// FIX: Annotate the listener method with @Transactional(propagation = Propagation.REQUIRES_NEW)
package com.example.platform.events.listener;

import org.springframework.stereotype.Component;
import org.springframework.transaction.annotation.Propagation;
import org.springframework.transaction.annotation.Transactional;
import org.springframework.transaction.event.TransactionPhase;
import org.springframework.transaction.event.TransactionalEventListener;

public record AccountDebitedEvent(String accountId, double amount) {}

@Component
public class AuditLogEventListener {

    private final AuditLogRepository auditRepository;

    public AuditLogEventListener(AuditLogRepository auditRepository) {
        this.auditRepository = auditRepository;
    }

    // REQUIRES_NEW opens a FRESH, independent database transaction for the audit entry
    @Transactional(propagation = Propagation.REQUIRES_NEW)
    @TransactionalEventListener(phase = TransactionPhase.AFTER_COMMIT)
    public void recordAuditTrail(AccountDebitedEvent event) {
        AuditLogEntry entry = new AuditLogEntry("DEBIT", event.accountId(), event.amount());
        auditRepository.save(entry);
    }
}

interface AuditLogRepository {
    void save(AuditLogEntry entry);
}

record AuditLogEntry(String action, String accountId, double amount) {}
```

### Example 4: Missing Fallback Execution When Transactions Are Absent (Anti-Pattern Fix)
```java
// ANTI-PATTERN: If a method is called outside a @Transactional boundary,
// @TransactionalEventListener drops the event SILENTLY by default!

// FIX: Set fallbackExecution = true if the listener must execute even without a transaction
@Component
public class UniversalMetricListener {

    @TransactionalEventListener(
            phase = TransactionPhase.AFTER_COMMIT,
            fallbackExecution = true // Ensures execution occurs even when no active transaction exists
    )
    public void recordMetric(OrderPlacedEvent event) {
        // Emits Prometheus metrics reliably in both transactional and non-transactional paths
    }
}
```

### Example 5: High-Performance Filtered Event Listener with SpEL
```java
package com.example.platform.events.listener;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.context.event.EventListener;
import org.springframework.stereotype.Component;

public record HighValueTransactionEvent(String transactionId, double amount, String currency) {}

@Component
public class FraudAlertEventListener {

    private static final Logger log = LoggerFactory.getLogger(FraudAlertEventListener.class);

    // SpEL condition: only triggers if amount is >= 10,000 USD
    @EventListener(condition = "#event.amount >= 10000.0 && #event.currency == 'USD'")
    public void flagSuspiciousTransaction(HighValueTransactionEvent event) {
        log.warn("ALERT: High-value transaction detected for fraud review: ID={}, Amount=${}",
                event.transactionId(), event.amount());
    }
}
```

---

## 6. Best Practices

1. **Always Use Java Records for Event Payloads**: Domain events represent immutable facts that occurred in the past; records provide canonical immutability without boilerplate.
2. **Combine with `@Async` for Heavy I/O**: External network calls (email, SMS, Kafka publishing) in `AFTER_COMMIT` listeners should be `@Async` to prevent delaying the completion of the user's thread.
3. **Use `Propagation.REQUIRES_NEW` for DB Writes in `AFTER_COMMIT`**: Because the original database transaction has already completed, any new database operations require their own independent transaction.
4. **Name Events in Past Tense**: Adhere to DDD conventions (`OrderCreatedEvent`, `PaymentCapturedEvent`, `InvoiceSentEvent`).
5. **Set `fallbackExecution = true` Intentionally**: Understand that without `fallbackExecution = true`, events published outside `@Transactional` methods are discarded silently.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Modifying database in `AFTER_COMMIT` without `@Transactional(REQUIRES_NEW)` | Original transaction is closed; Hibernate throws `TransactionRequiredException` or ignores flush | Add `@Transactional(propagation = Propagation.REQUIRES_NEW)` on the listener. |
| Event silently dropped when published outside `@Transactional` | Default `fallbackExecution = false` ignores events if no transaction is active | Set `@TransactionalEventListener(fallbackExecution = true)`. |
| Performing long-running I/O synchronously in `AFTER_COMMIT` | Holds the web container worker thread hostage after DB commit | Annotate listener with `@Async("taskExecutor")`. |
| Publishing mutable entity objects as events | Other threads or listeners can mutate the entity, creating race conditions | Publish immutable DTO records containing IDs and primitives, not JPA entities. |
| Expecting `BEFORE_COMMIT` to prevent commit without throwing an exception | Non-exceptional code allows transaction to proceed | Throw an explicit `RuntimeException` inside `BEFORE_COMMIT` to abort and rollback. |
| Calling `publishEvent()` inside `BEFORE_COMMIT` listener causing endless loops | Listener re-triggers itself, causing `StackOverflowError` | Isolate event publishing logic and guard against recursive invocations. |

---

## 8. Performance & Security Notes

- **In-Memory Event Overhead**: Standard `ApplicationEventPublisher` uses direct Java method dispatch; invocation overhead is negligible (nanoseconds).
- **Zero-Serialization Cost**: In-process events do not serialize to JSON or byte buffers, avoiding GC pressure.
- **Security Context Inheritance**: When combining `@TransactionalEventListener` with `@Async`, ensure security tokens are propagated via `DelegatingSecurityContextAsyncTaskExecutor`.

---

## 9. Comparison / When to Use What

| Feature | Standard `@EventListener` | `@TransactionalEventListener` | External Message Queue (Kafka/RabbitMQ) |
|---|---|---|---|
| **Execution Timing** | Immediate (synchronous) | Phase-dependent (e.g. `AFTER_COMMIT`) | Out-of-process distributed async |
| **Transaction Coupling** | Shares original transaction | Runs outside or after original transaction | Decoupled via Outbox pattern |
| **Scope** | Single JVM application context | Single JVM application context | Distributed across multiple clusters |
| **Persistence** | In-memory only (ephemeral) | In-memory only (ephemeral) | Persistent disk broker |
| **Best For** | Internal UI/cache updates | Post-commit side effects (emails, metrics)| Cross-microservice event streams |

---

## 10. Interview Questions

<details>
<summary>1. What is the fundamental problem that `@TransactionalEventListener` solves?</summary>
In a standard `@EventListener`, the listener executes immediately while the database transaction is still active. If the listener triggers an external side-effect (like charging a credit card or sending an email) and the database transaction subsequently fails and rolls back, the side-effect has already occurred, creating inconsistent state. `@TransactionalEventListener(phase = AFTER_COMMIT)` ensures the listener only executes after the transaction has successfully committed.
</details>

<details>
<summary>2. Why does modifying the database inside an `AFTER_COMMIT` listener fail without `Propagation.REQUIRES_NEW`?</summary>
By the time `AFTER_COMMIT` fires, the underlying `PlatformTransactionManager` has already committed and closed the active database transaction. Any attempt to write to repositories using the existing transaction context will fail silently or throw exceptions because no transaction is active. Adding `@Transactional(propagation = Propagation.REQUIRES_NEW)` instructs Spring to open a fresh, separate transaction for the listener.
</details>

<details>
<summary>3. What happens if an event is published from a method that is NOT annotated with `@Transactional`?</summary>
If the listener uses `@TransactionalEventListener` and no transaction is active, the event is silently ignored and the listener method does not execute. To force execution when transactions are absent, configure `fallbackExecution = true`.
</details>

<details>
<summary>4. What are the four available transaction phases in TransactionPhase?</summary>
1. `BEFORE_COMMIT`: Executes before the transaction commits (can vote to rollback by throwing an exception).
2. `AFTER_COMMIT` (Default): Executes after the transaction successfully commits.
3. `AFTER_ROLLBACK`: Executes only if the transaction rolls back.
4. `AFTER_COMPLETION`: Executes after transaction completion regardless of commit or rollback.
</details>

<details>
<summary>5. How does Spring implement Transactional Event Listeners internally?</summary>
Spring uses `TransactionSynchronizationManager`. When an event is published, the framework checks if an active transaction exists. If active, it registers a `TransactionSynchronization` callback with the transaction manager. The transaction manager calls `afterCommit()`, `afterCompletion()`, etc., which in turn invokes the listener method.
</details>

<details>
<summary>6. Why should domain events be modeled as Java records rather than JPA entities?</summary>
Domain events represent historical facts that cannot change. JPA entities are mutable, contain dirty-checking proxies, and can trigger lazy-loading exceptions outside active transactions. Records are immutable, serializable, and thread-safe.
</details>

<details>
<summary>7. How can you make a Transactional Event Listener execute asynchronously on a background thread?</summary>
By annotating the method with both `@Async` and `@TransactionalEventListener(phase = TransactionPhase.AFTER_COMMIT)`. Spring ensures the event is queued for the async executor only after the database commit succeeds.
</details>

<details>
<summary>8. How do you conditionally filter events using SpEL in an event listener?</summary>
Using the `condition` attribute: `@EventListener(condition = "#event.totalPrice > 100")`. Spring evaluates the SpEL expression against the incoming event parameter; if false, the listener invocation is skipped.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1 (Easy): Publish and consume a simple POJO event</summary>
Create a record `OrderCreatedEvent(String orderId)` and implement a `@Service` that publishes it and an `@EventListener` component that logs it.
</details>

<details>
<summary>Exercise 2 (Easy): Bind listener to AFTER_COMMIT</summary>
Update the listener from Exercise 1 to use `@TransactionalEventListener(phase = TransactionPhase.AFTER_COMMIT)`.
</details>

<details>
<summary>Exercise 3 (Medium): Execute asynchronous post-commit email dispatch</summary>
Write a service that persists a user in a `@Transactional` method, publishes `UserRegisteredEvent`, and an `@Async` listener that sends a simulated welcome email.
</details>

<details>
<summary>Exercise 4 (Medium): Write audit logs with REQUIRES_NEW</summary>
Implement an `AFTER_COMMIT` listener that saves an `AuditRecord` to a database repository using `@Transactional(propagation = Propagation.REQUIRES_NEW)`.
</details>

<details>
<summary>Exercise 5 (Hard): Implement compensating logic on AFTER_ROLLBACK</summary>
Create an order processing flow that reserves credits in a remote service. If the local database transaction rolls back, trigger an `AFTER_ROLLBACK` listener to release the remote credit reservation.
</details>

<details>
<summary>Exercise 6 (Hard): Integration test verifying transaction phase semantics</summary>
Write a `@SpringBootTest` test asserting that an `@TransactionalEventListener` does NOT fire when the test transaction rolls back, but DOES fire when the transaction commits.
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                       Spring Transactional Events Summary                       |
+---------------------------------------------------------------------------------+
| Event Definition:   public record OrderPlaced(UUID id, double amt) {}           |
| Publisher:          applicationEventPublisher.publishEvent(new OrderPlaced(...))|
| Default Listener:   @EventListener -> Executes immediately on caller thread     |
| Post-Commit:        @TransactionalEventListener(phase = AFTER_COMMIT)           |
| Rollback Handler:   @TransactionalEventListener(phase = AFTER_ROLLBACK)         |
| No-Tx Fallback:     @TransactionalEventListener(fallbackExecution = true)       |
| Database Writes:    Must use @Transactional(propagation = REQUIRES_NEW)         |
| Heavy Network I/O:  Combine with @Async("taskExecutor")                         |
| Invariant:          Never publish mutable JPA entities; use immutable records   |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Official Spring Application Events Documentation](https://docs.spring.io/spring-framework/reference/core/beans/context-introduction.html#context-functionality-events)
- [Spring Transaction Synchronization Reference](https://docs.spring.io/spring-framework/reference/data-access/transaction/declarative/tx-decl-explained.html)
- [Domain-Driven Design: Tackling Complexity in the Heart of Software (Evans)](https://www.domainlanguage.com/ddd/)
