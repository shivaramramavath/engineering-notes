# Distributed Transactions and Sagas: Choreography, Orchestration, and Compensations

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | Spring Boot 3, Kafka, Spring Data JPA, State Machines |

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
In monolithic systems, maintaining data consistency across multiple database tables is trivial: developers wrap business operations in a single local `@Transactional` boundary, relying on ACID guarantees provided by the database engine. In microservices where each service owns its private database (Database-per-Service), business transactions frequently span multiple microservices (e.g., e-commerce checkout spanning Order, Payment, Inventory, and Delivery services). Traditional distributed 2-Phase Commit (2PC / XA) protocols lock database records across network boundaries, introducing fatal coordinator bottlenecks, extreme latency, and vulnerability to network partitions.

### The Evolution
Hector Garcia-Molina and Kenneth Salem first formalized the **Saga Pattern** in 1987 as a mechanism for managing long-lived transactions without global locks. A Saga is a sequence of local transactions. Each local transaction updates the database within a single service and emits an event or message. The next service consumes this event and executes its local transaction. If any step fails (e.g., credit card declined), the Saga executes a series of **Compensating Transactions** that undo the changes made by preceding steps, achieving **Eventual Consistency**.

### Relevance in Modern Engineering
In cloud-native systems, Sagas are the only viable mechanism for coordinating complex multi-service workflows. Spring Boot engineers must master both **Choreography** (event-driven, decentralized) and **Orchestration** (command-driven, state machine controlled) Saga implementations with robust compensation mechanics.

---

## 2. Core Concepts

- **Saga**: A distributed transaction pattern broken into a sequence of local ACID transactions coordinated across multiple microservices.
- **Compensating Transaction**: An idempotent reverse operation that semantically cancels or undoes an earlier committed local transaction (e.g., refunding an authorized charge, restocking reserved items).
- **Pivot Transaction**: The point of no return in a Saga. Once the pivot transaction commits successfully, the Saga is guaranteed to proceed forward to completion without rolling back.
- **Retriable Transaction**: Steps following the pivot transaction that are guaranteed to eventually succeed through retries (e.g., sending an email receipt or generating a shipping label).
- **Choreography-Based Saga**: Decentralized coordination where each microservice listens to domain events from other services and independently decides its next local action without a central coordinator.
- **Orchestration-Based Saga**: Centralized coordination where a dedicated orchestrator service manages a state machine, explicitly issuing commands to participant microservices and coordinating compensations upon failure.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        Saga Choreography vs Orchestration                         |
|                                                                                   |
| [ Choreography (Decentralized Events) ]                                           |
|                                                                                   |
|  Order Service --------> Kafka: "order.created"                                   |
|                               |                                                   |
|                               +-------------------------> Payment Service         |
|                                                                |                  |
|  Order Service <-------- Kafka: "payment.failed" <-------------+                  |
|  (Rolls back: Status = CANCELLED)                                                 |
|                                                                                   |
| [ Orchestration (Centralized State Machine) ]                                     |
|                                                                                   |
|                               +-------------------------------+                   |
|                               |     Order Saga Orchestrator   |                   |
|                               |  State Machine: SAGA_PENDING  |                   |
|                               +-------------------------------+                   |
|                                  |                         ^                      |
|                 1. Command:      |                         | 2. Reply:            |
|                 ReserveStock     |                         | StockReserved        |
|                                  v                         |                      |
|                               +-------------------------------+                   |
|                               |       Inventory Service       |                   |
|                               +-------------------------------+                   |
|                                  |                                                |
|                 3. Command:      |                                                |
|                 ChargeCard       v                                                |
|                               +-------------------------------+                   |
|                               |        Payment Service        |                   |
|                               |   FAIL: Card Declined!        |                   |
|                               +-------------------------------+                   |
|                                  |                                                |
|                                  v 4. Reply: PaymentFailed                        |
|                 5. Compensate: ReleaseStock                                       |
|                 Orchestrator -> Inventory Service (Stock Released!)               |
|                 Orchestrator updates Saga state to COMPENSATED                    |
+-----------------------------------------------------------------------------------+
```

In Choreography, participant services subscribe to Kafka topics. When `OrderCreatedEvent` is published, `PaymentService` attempts to charge the card. If it fails, it publishes `PaymentFailedEvent`. `OrderService` listens to `PaymentFailedEvent` and cancels the order. In Orchestration, the `OrderSagaOrchestrator` holds a persistent state machine. It issues an explicit command to `InventoryService`, waits for acknowledgment, then issues a command to `PaymentService`. If payment fails, the orchestrator triggers the `ReleaseStockCommand` compensation to `InventoryService` before marking the Saga failed.

---

## 4. Syntax & Usage

### Maven Dependencies
```xml
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-data-jpa</artifactId>
</dependency>
<dependency>
    <groupId>org.springframework.kafka</groupId>
    <artifactId>spring-kafka</artifactId>
</dependency>
```

---

## 5. Practical Examples

### Example 1: Choreography-Based Saga (Payment Consumer Emitting Success or Failure)
```java
package com.example.platform.saga.choreography;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.kafka.annotation.KafkaListener;
import org.springframework.kafka.core.KafkaTemplate;
import org.springframework.stereotype.Component;

public record OrderCreatedEvent(String orderId, String customerId, double amount) {}
public record PaymentCompletedEvent(String orderId, String paymentId) {}
public record PaymentFailedEvent(String orderId, String reason) {}

@Component
public class PaymentSagaParticipant {

    private static final Logger log = LoggerFactory.getLogger(PaymentSagaParticipant.class);
    private final KafkaTemplate<String, Object> kafkaTemplate;

    public PaymentSagaParticipant(KafkaTemplate<String, Object> kafkaTemplate) {
        this.kafkaTemplate = kafkaTemplate;
    }

    @KafkaListener(topics = "orders.created", groupId = "payment-saga-group")
    public void handleOrderCreated(OrderCreatedEvent event) {
        log.info("Saga Step: Authorizing payment for order [{}] amount ${}", event.orderId(), event.amount());

        try {
            // Attempt to charge payment
            if (event.amount() > 5000.0) {
                // Simulate credit limit exceeded failure
                throw new IllegalStateException("Credit limit exceeded for amount: " + event.amount());
            }

            // Local transaction commit: record payment in DB...
            String paymentId = "pay_" + event.orderId();

            // Emit success event to advance the Saga
            kafkaTemplate.send("payments.completed", event.orderId(), new PaymentCompletedEvent(event.orderId(), paymentId));
            log.info("Payment succeeded for order [{}]. Emitted PaymentCompletedEvent.", event.orderId());

        } catch (Exception e) {
            log.warn("Payment failed for order [{}]: {}. Emitting PaymentFailedEvent.", event.orderId(), e.getMessage());
            // Emit failure event to trigger upstream compensations
            kafkaTemplate.send("payments.failed", event.orderId(), new PaymentFailedEvent(event.orderId(), e.getMessage()));
        }
    }
}
```

### Example 2: Choreography-Based Compensation Listener in Order Service
```java
package com.example.platform.saga.choreography;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.kafka.annotation.KafkaListener;
import org.springframework.stereotype.Component;
import org.springframework.transaction.annotation.Transactional;

@Component
public class OrderCompensationListener {

    private static final Logger log = LoggerFactory.getLogger(OrderCompensationListener.class);
    private final OrderRepository orderRepository;

    public OrderCompensationListener(OrderRepository orderRepository) {
        this.orderRepository = orderRepository;
    }

    // Handles compensating rollback when downstream payment fails
    @Transactional
    @KafkaListener(topics = "payments.failed", groupId = "order-compensation-group")
    public void compensateOrder(PaymentFailedEvent event) {
        log.warn("Executing COMPENSATING TRANSACTION for order [{}]. Reason: {}",
                event.orderId(), event.reason());

        // Idempotent state transition to CANCELLED
        orderRepository.updateOrderStatus(event.orderId(), "CANCELLED_PAYMENT_FAILED");
        log.info("Order [{}] successfully compensated to CANCELLED state", event.orderId());
    }
}

interface OrderRepository {
    void updateOrderStatus(String orderId, String status);
}
```

### Example 3: Centralized Saga Orchestrator Managing State Machine
```java
package com.example.platform.saga.orchestrator;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.time.Instant;
import java.util.UUID;

public enum SagaState {
    STARTED,
    INVENTORY_RESERVED,
    PAYMENT_CAPTURED,
    COMPLETED,
    COMPENSATING,
    FAILED
}

@Service
public class CheckoutSagaOrchestrator {

    private static final Logger log = LoggerFactory.getLogger(CheckoutSagaOrchestrator.class);

    private final InventoryRpcClient inventoryClient;
    private final PaymentRpcClient paymentClient;
    private final SagaLogRepository sagaRepository;

    public CheckoutSagaOrchestrator(
            InventoryRpcClient inventoryClient,
            PaymentRpcClient paymentClient,
            SagaLogRepository sagaRepository) {
        this.inventoryClient = inventoryClient;
        this.paymentClient = paymentClient;
        this.sagaRepository = sagaRepository;
    }

    @Transactional
    public boolean executeCheckoutSaga(String orderId, String sku, int qty, double amount) {
        UUID sagaId = UUID.randomUUID();
        log.info("Starting Saga [{}] for order [{}]", sagaId, orderId);
        sagaRepository.recordState(sagaId, orderId, SagaState.STARTED);

        // Step 1: Compensatable Transaction -> Reserve Inventory
        boolean stockReserved = inventoryClient.reserveStock(orderId, sku, qty);
        if (!stockReserved) {
            log.warn("Step 1 failed: Out of stock. Terminating Saga.");
            sagaRepository.recordState(sagaId, orderId, SagaState.FAILED);
            return false;
        }
        sagaRepository.recordState(sagaId, orderId, SagaState.INVENTORY_RESERVED);

        // Step 2: Pivot Transaction -> Charge Payment
        boolean paymentSuccess = paymentClient.charge(orderId, amount);
        if (!paymentSuccess) {
            log.error("Step 2 (Pivot) failed: Payment declined! Initiating COMPENSATIONS.");
            sagaRepository.recordState(sagaId, orderId, SagaState.COMPENSATING);

            // Execute Compensation: Undo Step 1
            inventoryClient.releaseStock(orderId, sku, qty);

            sagaRepository.recordState(sagaId, orderId, SagaState.FAILED);
            return false;
        }

        // Step 3: Retriable Transaction -> Complete
        sagaRepository.recordState(sagaId, orderId, SagaState.COMPLETED);
        log.info("Saga [{}] completed successfully", sagaId);
        return true;
    }
}

interface InventoryRpcClient { boolean reserveStock(String oId, String sku, int q); void releaseStock(String oId, String sku, int q); }
interface PaymentRpcClient { boolean charge(String oId, double amt); }
interface SagaLogRepository { void recordState(UUID sagaId, String orderId, SagaState state); }
```

### Example 4: The 2-Phase Commit (XA) in Microservices Anti-Pattern and Fix
```java
// ANTI-PATTERN: Attempting to use JTA / XA / Atomikos across microservices.
// Coordinator locks database rows across distributed network connections.
// If a network partition occurs, locks are held indefinitely, stalling all transactions!

// FIX: Replace distributed locks with the Saga pattern and eventual consistency
```

### Example 5: High-Performance Idempotent Compensation Handler
```java
package com.example.platform.saga.participant;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Component;
import org.springframework.transaction.annotation.Transactional;

@Component
public class StockCompensationHandler {

    private static final Logger log = LoggerFactory.getLogger(StockCompensationHandler.class);

    // Invariant: Compensating transactions MUST be 100% idempotent!
    // Safely callable multiple times without double-releasing stock
    @Transactional
    public void releaseReservedStockIdempotent(String orderId, String sku, int quantity) {
        log.info("Executing idempotent compensation for order [{}], SKU: {}", orderId, sku);

        boolean alreadyReleased = checkReservationStatus(orderId);
        if (alreadyReleased) {
            log.info("Stock for order [{}] was already released. Skipping duplicate compensation.", orderId);
            return;
        }

        // Restore inventory and mark reservation RELEASED
        incrementStock(sku, quantity);
        markReservationReleased(orderId);
    }

    private boolean checkReservationStatus(String orderId) { return false; }
    private void incrementStock(String sku, int q) {}
    private void markReservationReleased(String orderId) {}
}
```

---

## 6. Best Practices

1. **Make All Compensations Idempotent**: A compensating transaction may be executed multiple times due to message retries; it must produce the exact same outcome without side-effects.
2. **Clearly Identify the Pivot Transaction**: Explicitly design the transaction boundary where the Saga switches from "compensatable" (can rollback) to "retriable" (must complete forward).
3. **Use Orchestration for Complex Multi-Step Sagas ($> 4$ steps)**: Choreography becomes impossible to trace and debug when flows exceed 3-4 steps; use Orchestration for clear visibility and centralized control.
4. **Use Choreography for Simple 2-3 Step Workflows**: For small interactions (Order $\to$ Payment $\to$ Receipt), Choreography avoids the overhead of managing a dedicated orchestrator service.
5. **Persist Saga State Machine Transitions**: The orchestrator must record state changes in a durable database table before issuing commands to ensure recovery across orchestrator restarts.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Attempting 2-Phase Commit (XA) across microservices | Network partitions leave distributed locks held, crashing database throughput | Migrate to Sagas with local ACID transactions and compensations. |
| Non-idempotent compensating transactions | If compensation is retried, it charges or refunds the customer twice | Enforce idempotency using unique reservation or order IDs. |
| Cyclic dependencies in Saga Choreography | Services listen to each other in circles, causing infinite event cascades | Break cycles or switch to centralized Saga Orchestration. |
| Forgetting to handle orchestrator crash | In-flight Saga is orphaned mid-execution if the orchestrator restarts | Persist Saga state transitions to a database table; resume pending Sagas on boot. |
| Inability to compensate downstream legacy systems | Some legacy systems cannot perform automated reversals | Isolate legacy calls to the final, retriable step, or use human-in-the-loop workflows. |
| Ignoring concurrency race conditions during compensation | User modifies order while compensating transaction is in flight | Introduce semantic locking (e.g., status `PENDING_CANCELLATION`). |

---

## 8. Performance & Security Notes

- **Zero Database Lock Contention**: Sagas eliminate distributed locks entirely; each step commits its local database transaction immediately, maintaining high throughput.
- **Semantic Locking (Dirty Reads)**: Because intermediate local transactions commit before the entire Saga finishes, other transactions can read unfinalized data. Mitigate by flagging entities with status `PENDING_APPROVAL`.
- **Command Injection Prevention**: Encrypt and cryptographically sign Saga command messages traversing Kafka topics to prevent tampering with financial compensation instructions.

---

## 9. Comparison / When to Use What

| Feature | Saga Choreography | Saga Orchestration | 2-Phase Commit (2PC / XA) |
|---|---|---|---|
| **Coordination** | Decentralized (Event Pub/Sub) | Centralized (Command State Machine)| Centralized Transaction Coordinator |
| **Coupling** | Loose (Services know events only)| Moderate (Orchestrator knows flow) | Extremely Tight (DB-level locks) |
| **Complexity** | Simple for small flows | Better for complex enterprise flows| Extremely High |
| **Visibility / Tracing** | Difficult to visualize full flow | Easy to monitor in single dashboard| Opaque |
| **Best For** | 2-3 step microservice workflows | Complex financial/e-commerce Sagas | Monolithic databases only |

---

## 10. Interview Questions

<details>
<summary>1. What is the Saga Pattern and why is it preferred over 2-Phase Commit (2PC) in microservices?</summary>
A Saga is an architectural pattern for managing distributed transactions as a sequence of local transactions, coordinated via events or commands, with compensating transactions for rollback. It is preferred over 2-Phase Commit because 2PC requires distributed locks across multiple database engines, creating single points of failure, high latency, and deadlocks during network partitions. Sagas commit local transactions immediately, maintaining high availability and scalability.
</details>

<details>
<summary>2. What is the difference between Saga Choreography and Saga Orchestration?</summary>
In Choreography, participant services publish and listen to domain events, deciding their next local actions autonomously without a central coordinator. In Orchestration, a dedicated orchestrator service manages a state machine, explicitly issuing command messages to participants and directing compensating actions if a step fails.
</details>

<details>
<summary>3. What is a Pivot Transaction in a Saga?</summary>
The Pivot Transaction is the dividing line in a Saga. Steps *before* the pivot transaction are compensatable (if they fail, preceding steps must be undone). Once the pivot transaction successfully commits, the Saga can no longer be rolled back; all subsequent steps must be *retriable* transactions that are guaranteed to eventually succeed.
</details>

<details>
<summary>4. What are the key requirements of a Compensating Transaction?</summary>
A compensating transaction must be:
1. **Idempotent**: Safe to be retried multiple times without side-effects.
2. **Guaranteed to Succeed**: It must not fail due to business validation (failures must be retried until successful or escalated to human operators).
3. **Semantically Correct**: It undos the business impact of an earlier transaction (e.g., crediting an account rather than hard-deleting an immutable ledger entry).
</details>

<details>
<summary>5. How do you handle the problem of "Dirty Reads" (lack of isolation) in Sagas?</summary>
Because each local transaction commits immediately, other concurrent requests can see intermediate, uncommitted states (ACID Isolation violation). To prevent errors, systems use **Semantic Locks**: setting entity status to `PENDING` (e.g., `ORDER_PENDING_PAYMENT`), preventing other operations from modifying the entity until the Saga reaches a terminal state.
</details>

<details>
<summary>6. How does an Orchestration Saga recover if the Orchestrator service crashes mid-transaction?</summary>
The orchestrator must record state transitions (e.g., `INVENTORY_RESERVED`) in a persistent database table before issuing the next command. When the orchestrator recovers or a replica takes over, a background worker scans for uncompleted Sagas and resumes execution from the last persisted state.
</details>

<details>
<summary>7. When does Saga Choreography become an anti-pattern?</summary>
When a workflow spans many microservices ($> 4$ services) with complex branching logic. In large workflows, Choreography creates "spaghetti events" where no single developer understands the complete end-to-end flow, circular event dependencies emerge, and debugging failure scenarios becomes exceedingly difficult.
</details>

<details>
<summary>8. How does the Transactional Outbox pattern complement Sagas?</summary>
The Transactional Outbox pattern guarantees that each participant service updates its local database and publishes its Saga event/reply atomically without dual-write risks. Without Outbox, a participant could commit its local transaction and crash before emitting its event, permanently stalling the Saga.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1 (Easy): Model Saga states as Java enum</summary>
Define a Java `enum SagaStatus` with values `PENDING`, `INVENTORY_RESERVED`, `PAYMENT_COMPLETED`, `COMPENSATING`, `FAILED`.
</details>

<details>
<summary>Exercise 2 (Easy): Define a compensating event record</summary>
Create a record `CancelOrderEvent` containing order ID and cancellation reason.
</details>

<details>
<summary>Exercise 3 (Medium): Implement an idempotent stock release method</summary>
Write a Spring service method `void releaseStock(String orderId, String sku, int qty)` that checks a released flag to prevent double-restocking.
</details>

<details>
<summary>Exercise 4 (Medium): Build a Choreography event listener</summary>
Implement a listener that handles `PaymentFailedEvent` and executes an update query setting order status to `CANCELLED`.
</details>

<details>
<summary>Exercise 5 (Hard): Implement an Orchestrator with database state tracking</summary>
Write an `OrderSagaOrchestrator` that persists state transitions in a `saga_instances` table and executes a 2-step command pipeline with compensation.
</details>

<details>
<summary>Exercise 6 (Hard): Integration test simulating mid-Saga payment failure</summary>
Write a `@SpringBootTest` test that initiates a Saga, mocks a payment failure, and asserts that the inventory compensation method was invoked and the order was canceled.
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                       Distributed Sagas Quick Reference                         |
+---------------------------------------------------------------------------------+
| Problem Solved:     Cross-microservice transactions without distributed locks   |
| 2PC Invariant:      NEVER use 2PC/XA across microservices; adopt Sagas          |
| Choreography:       Decentralized event pub/sub (Simple 2-3 step workflows)     |
| Orchestration:      Centralized state machine coordinator (Complex workflows)   |
| Pivot Transaction:  Point of no return; subsequent steps must succeed           |
| Compensations:      Must be 100% IDEMPOTENT (Safe for repeated execution)       |
| Isolation Defense:  Semantic locking (e.g. status = PENDING_AUTHORIZATION)      |
| Outbox Coupling:    Use Transactional Outbox to publish Saga events reliably    |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Sagas Paper (Hector Garcia-Molina & Kenneth Salem, 1987)](https://www.cs.cornell.edu/andru/cs711/2002fa/reading/sagas.pdf)
- [Microservices Patterns: Distributed Data Management (Chris Richardson)](https://microservices.io/patterns/data/saga.html)
- [Temporal.io / Camunda Workflow Orchestration Engines](https://temporal.io/)
