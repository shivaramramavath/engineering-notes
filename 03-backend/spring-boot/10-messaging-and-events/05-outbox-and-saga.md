# Distributed Data Consistency: Transactional Outbox and Saga Orchestration

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | PostgreSQL, Kafka, Spring Data JPA, Virtual Threads |

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
In distributed microservice architectures where each service owns its private database (Database-per-Service pattern), traditional 2-Phase Commit (2PC / XA distributed transactions) is practically unviable. 2PC introduces crippling latency, coordinator single-points-of-failure, and locks database rows across network partitions, violating the CAP theorem. Furthermore, the dual-write problem is notorious: if an application updates its database and subsequently publishes an event to Apache Kafka, a crash between the two operations leaves the database updated but the message never sent, permanently desynchronizing downstream microservices.

### The Evolution
To solve the dual-write problem, the industry adopted the **Transactional Outbox Pattern**: database mutations and outbound event messages are written to the *same local database* within a single, atomic ACID transaction. A separate relay process (polling worker or Change Data Capture engine like Debezium) reads the outbox table and dispatches events to the message broker. To manage long-running multi-service business transactions, systems implement the **Saga Pattern**—a sequence of local transactions coordinated via events (Choreography) or a centralized state machine (Orchestration) that executes compensating transactions if a step fails.

### Relevance in Modern Engineering
In fintech, e-commerce, and high-reliability systems, achieving eventual consistency without data loss is paramount. Spring Boot developers must master both Transactional Outbox implementations (polling and Debezium CDC) and Saga orchestrators to guarantee zero data loss and automated failure recovery.

---

## 2. Core Concepts

- **Dual-Write Problem**: The architectural defect where updating a database and publishing a message cannot be wrapped in a single non-distributed transaction, leading to inconsistent distributed states.
- **Transactional Outbox Pattern**: Persisting domain entity mutations and outbound event records together into an `outbox` table in the local database within a single atomic `@Transactional` boundary.
- **Message Relay**:
  - *Polling Publisher*: A `@Scheduled` background worker polling un-dispatched outbox records using `SELECT ... FOR UPDATE SKIP LOCKED`.
  - *Transaction Log Tailing (CDC)*: A tool (like Debezium) reading the database write-ahead log (Postgres WAL / MySQL Binlog) to stream changes to Kafka with zero application polling overhead.
- **Saga Pattern**: An architectural pattern managing distributed transactions through a sequence of local transactions, where every local step commits immediately and publishes an event triggering the next step.
- **Compensating Transaction**: An idempotent reverse transaction executed to undo the effects of an earlier committed local step if a subsequent step in the Saga fails (e.g., refunding a credit card if inventory allocation fails).
- **Choreography vs Orchestration**:
  - *Choreography*: Services react to each other's events independently without a central coordinator.
  - *Orchestration*: A dedicated orchestrator service coordinates the exact execution order and manages compensating rollbacks.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        Transactional Outbox & Saga Flow                           |
|                                                                                   |
| 1. Business Transaction Commits Atomically                                        |
|    @Transactional                                                                 |
|    public void placeOrder(...) {                                                  |
|        orderRepository.save(order);                                               |
|        outboxRepository.save(new OutboxRecord("OrderCreated", payloadJson));      |
|    } // Single Local ACID Transaction Commits! Zero Dual-Write Risk!              |
|                   |                                                               |
|                   v                                                               |
| 2. Outbox Relay (Polling Worker or Debezium CDC)                                  |
|    +---------------------------------------------------------------------+        |
|    | SELECT * FROM outbox WHERE status = 'PENDING'                       |        |
|    | FOR UPDATE SKIP LOCKED                                              |        |
|    +---------------------------------------------------------------------+        |
|                   |                                                               |
|                   v Publishes to Message Broker                                   |
|    Kafka Broker: topic = "orders.events"                                          |
|                   |                                                               |
|                   +---> Update outbox record status = 'SENT'                      |
|                                                                                   |
| 3. Saga Orchestrator Pipeline                                                     |
|    Step 1: Order Service (Created) ---> Local Commit                              |
|                   |                                                               |
|                   v Success                                                       |
|    Step 2: Payment Service (Charge Card) ---> Local Commit                        |
|                   |                                                               |
|                   v Fails (Card Declined!)                                        |
|    Step 3: Trigger Compensation                                                   |
|    Compensate Step 1: Order Service (Cancel Order) ---> Local Commit              |
+-----------------------------------------------------------------------------------+
```

When an order is created, the service inserts the order entity and an outbox record inside the same database transaction. A separate Spring worker polls the outbox table using `SELECT ... FOR UPDATE SKIP LOCKED` (allowing concurrent workers to pick up unreserved batches without row contention). Once Kafka acknowledges delivery, the outbox record is marked `PROCESSED` or deleted. If a downstream step in the Saga fails (e.g., payment declined), a compensating event is dispatched back, triggering the Order Service to cancel the order.

---

## 4. Syntax & Usage

### Outbox Database Schema (PostgreSQL DDL)
```sql
CREATE TABLE outbox_events (
    id UUID PRIMARY KEY,
    aggregate_type VARCHAR(255) NOT NULL,
    aggregate_id VARCHAR(255) NOT NULL,
    event_type VARCHAR(255) NOT NULL,
    payload JSONB NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL,
    status VARCHAR(50) NOT NULL,
    retry_count INT DEFAULT 0
);

CREATE INDEX idx_outbox_pending ON outbox_events (created_at) WHERE status = 'PENDING';
```

---

## 5. Practical Examples

### Example 1: Domain Service Writing to Outbox Atomically
```java
package com.example.platform.outbox.service;

import com.example.platform.outbox.entity.OutboxRecord;
import com.example.platform.outbox.repository.OutboxRepository;
import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.ObjectMapper;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.time.Instant;
import java.util.UUID;

public record Order(String id, String customerId, double amount, String status) {}
public record OrderCreatedEvent(String orderId, String customerId, double amount) {}

@Service
public class OrderApplicationService {

    private final OutboxRepository outboxRepository;
    private final ObjectMapper objectMapper;

    public OrderApplicationService(OutboxRepository outboxRepository, ObjectMapper objectMapper) {
        this.outboxRepository = outboxRepository;
        this.objectMapper = objectMapper;
    }

    @Transactional
    public Order placeOrder(String customerId, double amount) {
        String orderId = UUID.randomUUID().toString();
        Order order = new Order(orderId, customerId, amount, "PENDING");

        // 1. Persist domain entity
        saveOrderToDatabase(order);

        // 2. Persist outbound event in outbox table within the SAME transaction
        try {
            OrderCreatedEvent event = new OrderCreatedEvent(orderId, customerId, amount);
            String payloadJson = objectMapper.writeValueAsString(event);

            OutboxRecord outbox = new OutboxRecord(
                    UUID.randomUUID(),
                    "Order",
                    orderId,
                    "OrderCreated",
                    payloadJson,
                    Instant.now(),
                    "PENDING",
                    0
            );

            outboxRepository.save(outbox);
        } catch (JsonProcessingException e) {
            throw new IllegalStateException("Failed to serialize outbox event", e);
        }

        return order;
    }

    private void saveOrderToDatabase(Order order) {
        // JPA or JDBC save
    }
}
```

### Example 2: Outbox Polling Relay with `SKIP LOCKED`
```java
package com.example.platform.outbox.relay;

import com.example.platform.outbox.entity.OutboxRecord;
import com.example.platform.outbox.repository.OutboxRepository;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.kafka.core.KafkaTemplate;
import org.springframework.scheduling.annotation.Scheduled;
import org.springframework.stereotype.Component;
import org.springframework.transaction.annotation.Transactional;

import java.util.List;

@Component
public class OutboxMessageRelay {

    private static final Logger log = LoggerFactory.getLogger(OutboxMessageRelay.class);
    private final OutboxRepository outboxRepository;
    private final KafkaTemplate<String, String> kafkaTemplate;

    public OutboxMessageRelay(OutboxRepository outboxRepository, KafkaTemplate<String, String> kafkaTemplate) {
        this.outboxRepository = outboxRepository;
        this.kafkaTemplate = kafkaTemplate;
    }

    @Scheduled(fixedDelay = 1000) // Polls every second
    @Transactional
    public void publishPendingOutboxRecords() {
        // SELECT * FROM outbox_events WHERE status = 'PENDING' ORDER BY created_at LIMIT 50 FOR UPDATE SKIP LOCKED
        List<OutboxRecord> pendingEvents = outboxRepository.findTop50PendingForUpdate();

        for (OutboxRecord record : pendingEvents) {
            try {
                String topic = "events." + record.getAggregateType().toLowerCase();
                
                // Synchronously await or attach non-blocking future callback
                kafkaTemplate.send(topic, record.getAggregateId(), record.getPayload()).get();

                record.setStatus("SENT");
                log.info("Outbox event [{}] successfully relayed to Kafka topic [{}]", record.getId(), topic);
            } catch (Exception e) {
                log.error("Failed to relay outbox event [{}]", record.getId(), e);
                record.setRetryCount(record.getRetryCount() + 1);
                if (record.getRetryCount() > 5) {
                    record.setStatus("FAILED");
                }
            }
        }
    }
}
```

### Example 3: Outbox Repository with Native `FOR UPDATE SKIP LOCKED`
```java
package com.example.platform.outbox.repository;

import com.example.platform.outbox.entity.OutboxRecord;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;
import org.springframework.stereotype.Repository;

import java.util.List;
import java.util.UUID;

@Repository
public interface OutboxRepository extends JpaRepository<OutboxRecord, UUID> {

    // SKIP LOCKED prevents multiple concurrent relay threads/pods from locking the same rows
    @Query(value = """
            SELECT * FROM outbox_events 
            WHERE status = 'PENDING' 
            ORDER BY created_at ASC 
            LIMIT 50 
            FOR UPDATE SKIP LOCKED
            """, nativeQuery = true)
    List<OutboxRecord> findTop50PendingForUpdate();
}
```

### Example 4: The Dual-Write Anti-Pattern and Fix
```java
// ANTI-PATTERN: Updating the database and publishing to Kafka in the same method.
// If the JVM crashes, power fails, or network drops right after db.save(),
// the Kafka event is NEVER sent, permanently breaking downstream systems!
// @Transactional
// public void badDualWrite(Order order) {
//     db.save(order);
//     kafkaTemplate.send("orders", order); // CRASH HERE = DATA LOSS!
// }

// FIX: Commit entity and OutboxRecord in one local DB transaction; let Outbox relay handle Kafka
```

### Example 5: Saga Orchestrator Managing Distributed Rollback
```java
package com.example.platform.saga.orchestrator;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;

@Service
public class OrderSagaOrchestrator {

    private static final Logger log = LoggerFactory.getLogger(OrderSagaOrchestrator.class);

    private final PaymentClient paymentClient;
    private final InventoryClient inventoryClient;
    private final OrderRepository orderRepository;

    public OrderSagaOrchestrator(PaymentClient paymentClient, InventoryClient inventoryClient, OrderRepository orderRepository) {
        this.paymentClient = paymentClient;
        this.inventoryClient = inventoryClient;
        this.orderRepository = orderRepository;
    }

    public boolean executeOrderSaga(String orderId, String customerId, double amount, String sku) {
        log.info("Starting Saga for order: {}", orderId);

        // Step 1: Reserve Inventory
        boolean inventoryReserved = inventoryClient.reserve(orderId, sku, 1);
        if (!inventoryReserved) {
            log.warn("Saga Step 1 failed: Inventory reservation denied. Aborting Saga.");
            orderRepository.updateStatus(orderId, "FAILED_INSUFFICIENT_INVENTORY");
            return false;
        }

        // Step 2: Process Payment
        boolean paymentCaptured = paymentClient.charge(orderId, customerId, amount);
        if (!paymentCaptured) {
            log.error("Saga Step 2 failed: Payment declined. Initiating COMPENSATING TRANSACTIONS.");
            
            // Compensating Transaction: Release Inventory
            inventoryClient.releaseReservation(orderId, sku, 1);
            
            orderRepository.updateStatus(orderId, "CANCELED_PAYMENT_FAILED");
            return false;
        }

        // Step 3: Complete Order
        orderRepository.updateStatus(orderId, "CONFIRMED");
        log.info("Saga completed successfully for order: {}", orderId);
        return true;
    }
}

interface PaymentClient { boolean charge(String oId, String cId, double amt); }
interface InventoryClient { boolean reserve(String oId, String sku, int qty); void releaseReservation(String oId, String sku, int qty); }
interface OrderRepository { void updateStatus(String oId, String status); }
```

---

## 6. Best Practices

1. **Use `FOR UPDATE SKIP LOCKED` for Polling Relays**: When multiple application instances run concurrently, `SKIP LOCKED` ensures workers never block each other or process duplicate outbox rows.
2. **Make Event Consumers Idempotent**: The Transactional Outbox pattern guarantees **At-Least-Once** delivery. If a crash occurs after publishing to Kafka but before updating the outbox status, the event will be re-sent. Consumers must handle duplicates gracefully.
3. **Store Payloads as Canonical JSON**: Store outbox payloads as standard JSONB (Postgres) or text to maintain readability and enable CDC tailing by Debezium.
4. **Clean Up Processed Outbox Records**: Implement a scheduled maintenance job or partition dropping strategy to purge `SENT` outbox records so the table does not grow indefinitely.
5. **Design Explicit Compensating Transactions**: Every mutating step in a Saga must have a well-tested, idempotent compensating reverse action (e.g., `reserveInventory` $\to$ `releaseInventory`).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Committing DB write and publishing to Kafka without Outbox | Network failure after DB commit results in lost event (Dual-write hazard) | Persist event in `outbox_events` table in the same local DB transaction. |
| Polling outbox table without `SKIP LOCKED` | Concurrent worker pods encounter row lock contention, deadlocking each other | Use native query with `FOR UPDATE SKIP LOCKED`. |
| Assuming Outbox guarantees exactly-once delivery | Network resets between Kafka ACK and DB status update cause re-delivery | Build idempotent consumers using message deduplication keys. |
| Forgetting to purge processed outbox records | Outbox table grows to millions of rows, degrading query performance | Run `@Scheduled` purge job deleting records where `status = 'SENT'` older than 7 days. |
| Designing non-idempotent compensating actions | Retrying a failed compensation charges or releases inventory multiple times | Make compensating transactions idempotent using correlation IDs. |
| Mixing business logic into the Outbox Relay | Heavy processing in the relay delays event publishing for all services | Keep the relay lightweight: read from outbox, publish to Kafka, mark sent. |

---

## 8. Performance & Security Notes

- **Debezium vs Polling**: Polling creates database query load every second. High-throughput platforms deploy Debezium CDC, tailing the database WAL with zero polling overhead.
- **Outbox Table Partitioning**: In high-volume systems ($> 10,000$ tx/sec), partition the outbox table by day or week; old partitions can be dropped via `DROP TABLE` in milliseconds without vacuum overhead.
- **Event Payload Sanitization**: Never write unencrypted sensitive customer data (passwords, plaintext credit card numbers) into outbox payload columns.

---

## 9. Comparison / When to Use What

| Pattern | Transactional Outbox | 2-Phase Commit (2PC / XA) | Saga Choreography | Saga Orchestration |
|---|---|---|---|---|
| **Consistency** | Eventual consistency | Strict ACID | Eventual consistency | Eventual consistency |
| **Complexity** | Low / Moderate | High (Coordinator locks) | Moderate | Moderate / High |
| **Availability** | High (Local DB only) | Low (Blocks on network) | High | High |
| **Rollback Mechanism** | Retries in Relay | Automatic abort | Event-driven compensation | Orchestrator compensation |
| **Best For** | Zero dual-write risk | Single monolith across DBs | Simple 2-3 step workflows | Complex multi-step enterprise Sagas |

---

## 10. Interview Questions

<details>
<summary>1. What is the Dual-Write Problem in microservices?</summary>
The Dual-Write Problem occurs when an application must update its local database and notify other systems by sending a message to a broker (like Kafka). Because a local database transaction cannot encompass an external network call to a message broker, a failure or crash between the two actions results in an inconsistent state: either the database is updated without an event, or an event is published for a transaction that was subsequently rolled back.
</details>

<details>
<summary>2. How does the Transactional Outbox pattern solve the Dual-Write Problem?</summary>
By replacing the remote broker call with a local database insert. The domain entity changes and the outbound event message are written to the local database within the exact same atomic ACID transaction. A separate relay process reads the outbox table and reliably publishes events to the broker.
</details>

<details>
<summary>3. Why is `FOR UPDATE SKIP LOCKED` essential for database polling relays?</summary>
When multiple instances of an application run in Kubernetes, each instance runs a background thread polling the outbox table. Standard `SELECT ... FOR UPDATE` locks rows and forces other threads to block, creating lock contention and deadlocks. `SKIP LOCKED` instructs the database engine to skip rows already locked by other transactions, allowing concurrent workers to process distinct batches in parallel.
</details>

<details>
<summary>4. What delivery guarantee does the Transactional Outbox pattern provide?</summary>
The Transactional Outbox pattern provides **At-Least-Once** delivery. If the relay publishes an event to Kafka but the application crashes before updating the outbox record's status to `SENT`, the next polling cycle will re-read and re-publish the message. Consumers must be designed to be idempotent.
</details>

<details>
<summary>5. How does a Change Data Capture (CDC) engine like Debezium improve upon a polling outbox relay?</summary>
A polling relay repeatedly executes `SELECT` queries against the database, consuming database CPU and I/O even when no events are pending. Debezium reads the database transaction log directly (Postgres WAL or MySQL Binlog), streaming new outbox rows to Kafka in real time with sub-millisecond latency and zero database query overhead.
</details>

<details>
<summary>6. What is a Saga and how does it maintain consistency across microservices?</summary>
A Saga is a sequence of local transactions. Each transaction updates data within a single service and publishes an event or message to trigger the next transaction in a downstream service. If a local transaction fails, the Saga executes a series of compensating transactions to undo the changes made by preceding local transactions.
</details>

<details>
<summary>7. What is the difference between Saga Choreography and Saga Orchestration?</summary>
In Choreography, services communicate by listening to domain events and deciding their next actions autonomously without a central coordinator. In Orchestration, a central orchestrator service coordinates the exact sequence of steps, invokes each participant, and centrally manages compensating rollbacks if any step fails.
</details>

<details>
<summary>8. What is a Compensating Transaction and what are its key requirements?</summary>
A compensating transaction is a reverse operation that semantically undoes an earlier committed local transaction (e.g., refunding a charge or releasing a seat reservation). Key requirements: it must be completely idempotent (safe to retry), must not fail, and must accept that the world may have changed between the original action and the compensation.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1 (Easy): Create an OutboxRecord JPA entity</summary>
Write a JPA entity `OutboxRecord` with fields: `id` (UUID), `aggregateType`, `aggregateId`, `eventType`, `payload` (String), `createdAt`, and `status`.
</details>

<details>
<summary>Exercise 2 (Easy): Implement atomic outbox insertion</summary>
Write a `@Transactional` service method that saves an `Account` entity and persists an `AccountCreated` outbox event in the same transaction.
</details>

<details>
<summary>Exercise 3 (Medium): Implement an Outbox Polling Relay</summary>
Write a `@Scheduled` method that retrieves pending outbox records, sends them to Kafka via `KafkaTemplate`, and marks them `SENT`.
</details>

<details>
<summary>Exercise 4 (Medium): Build a query with FOR UPDATE SKIP LOCKED</summary>
Write a Spring Data JPA repository method with native query retrieving the oldest 20 pending outbox records using `FOR UPDATE SKIP LOCKED`.
</details>

<details>
<summary>Exercise 5 (Hard): Implement a multi-step Saga Orchestrator</summary>
Write an `OrderSagaOrchestrator` coordinating payment authorization, stock reservation, and shipping dispatch with automated compensation on payment failure.
</details>

<details>
<summary>Exercise 6 (Hard): Integration test testing Outbox relay recovery</summary>
Write a `@SpringBootTest` test asserting that an outbox record is persisted, picked up by the relay, published to an embedded Kafka topic, and updated to `SENT`.
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                    Transactional Outbox & Saga Summary                          |
+---------------------------------------------------------------------------------+
| Problem Solved:     Dual-Write Problem (DB update + message publish data loss)  |
| Outbox Pattern:     Save entity + OutboxRecord in SAME local ACID transaction   |
| Relay Query:        SELECT ... FOR UPDATE SKIP LOCKED                           |
| Delivery Guarantee: At-Least-Once (Consumers MUST be idempotent)                |
| CDC Alternative:    Debezium tails Postgres WAL directly into Kafka             |
| Saga Pattern:       Sequence of local transactions with compensating rollbacks  |
| Choreography:       Decentralized event reaction across services                |
| Orchestration:      Centralized state machine coordinating steps and rollbacks  |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Microservices Patterns: With examples in Java (Chris Richardson)](https://microservices.io/patterns/data/transactional-outbox.html)
- [Debezium Architecture Guide](https://debezium.io/documentation/reference/stable/architecture.html)
- [SAGA Pattern in Distributed Systems (AWS Architecture Blog)](https://aws.amazon.com/blogs/compute/building-a-serverless-distributed-application-using-a-saga-pattern/)
