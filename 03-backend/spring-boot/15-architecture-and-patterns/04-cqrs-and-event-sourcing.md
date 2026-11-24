# CQRS and Event Sourcing with Spring Boot

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.x |
| **Tested With** | PostgreSQL, Axon Framework, Kafka, Spring Data JDBC, Redis |

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
Traditional CRUD applications use a single unified data model for both reads and writes. As enterprise systems scale, this shared model creates severe structural tension:
1. **Write operations** demand strict relational normalization, locking, transaction isolation, and invariant validation.
2. **Read operations** demand multi-table joins, aggregations, denormalized projections, and sub-millisecond query responses.
Optimizing database indexes and ORM models for complex reporting degrades transactional write throughput, while optimizing for high-volume writes makes query performance intolerable.

### The Evolution
- **Command Query Responsibility Segregation (CQRS)**, pioneered by Greg Young and Bertrand Meyer, splits the application into two distinct paths: a **Write/Command side** (intent-revealing commands, domain invariants, transactions) and a **Read/Query side** (optimized denormalized views).
- **Event Sourcing (ES)** replaces traditional mutable database state updates (`UPDATE orders SET status = 'SHIPPED'`) with an append-only log of immutable domain events (`OrderPlaced`, `PaymentReceived`, `OrderShipped`). Current state is derived by replaying past events.

### Production Relevance
CQRS and Event Sourcing are critical in fintech, e-commerce order management, healthcare auditing, logistics tracking, and collaborative multi-user applications where complete audit trails, time-travel debugging, and asymmetric read/write scaling are mandatory.

---

## 2. Core Concepts

### Architectural Topology of CQRS & Event Sourcing

```
+-----------------------------------------------------------------------------------------+
|                               CQRS & EVENT SOURCING MODEL                               |
+-----------------------------------------------------------------------------------------+
|                                                                                         |
|       [ HTTP POST / PUT / DELETE ]                  [ HTTP GET / SEARCH ]               |
|                    |                                             |                      |
|                    v                                             v                      |
|       +-------------------------+                   +-------------------------+         |
|       |     COMMAND MODEL       |                   |       QUERY MODEL       |         |
|       |  (Invariants, Business) |                   | (Projections, Fast Read)|         |
|       +-------------------------+                   +-------------------------+         |
|                    |                                             ^                      |
|                    v (Append Event)                              |                      |
|       +-------------------------+                                | (SQL / Key-Value)    |
|       |       EVENT STORE       |                                |                      |
|       | (PostgreSQL / EventStore|                                |                      |
|       |  Append-Only Log Table) |                                |                      |
|       +-------------------------+                                |                      |
|                    |                                             |                      |
|                    | Emits Domain Event                          |                      |
|                    v                                             |                      |
|       +----------------------------------------------------------+                      |
|       |                     PROJECTION ENGINE                                           |
|       |          (@TransactionalEventListener / Kafka Consumer)                         |
|       |            Updates Denormalized Read Views (Elastic / Redis)                    |
|       +---------------------------------------------------------------------------------+
|                                                                                         |
+-----------------------------------------------------------------------------------------+
```

### Key Architectural Definitions
1. **Command**: An imperative request to alter state (e.g., `PlaceOrderCommand`, `CancelSubscriptionCommand`). Commands can be rejected if business invariants are violated.
2. **Query**: A pure request for data that never alters system state (e.g., `GetOrderSummaryQuery`, `SearchCatalogQuery`).
3. **Event**: An immutable statement of fact that occurred in the past (e.g., `OrderPlacedEvent`). Events cannot be rejected or modified.
4. **Event Store**: An append-only persistence store optimized for storing streams of events partitioned by aggregate ID.
5. **Read Projection (Read Model)**: A denormalized materialized view (PostgreSQL table, Redis hash, Elasticsearch index) tailored to specific UI/API query requirements.
6. **Eventual Consistency**: The brief window between an event being appended to the Event Store and the projection engine updating the Read Model.

---

## 3. How It Works Internally

### The Event Sourced Aggregate Reconstitution Lifecycle

```
[ Incoming Command: ConfirmPayment(orderId) ]
       |
       v
1. Load all historical events for aggregate: "order-{uuid}"
       |
       +---> [ EventStore ] -> [ Event 1: OrderCreated ]
       |                    -> [ Event 2: ItemAdded    ]
       v
2. Replay events in-memory to reconstruct current Aggregate state:
       Order order = new Order();
       order.apply(Event 1); // status = PENDING
       order.apply(Event 2); // items.add(...)
       |
       v
3. Execute business logic on reconstituted state:
       order.confirmPayment(paymentRef); // Checks: is status == PENDING?
       |
       v
4. Generate new event: [ Event 3: PaymentConfirmed ]
       |
       v
5. Append Event 3 to EventStore with Optimistic Concurrency Check (expectedVersion = 2)
       |
       v
6. Projector updates Read Database asynchronously:
       UPDATE order_summary_views SET payment_status = 'PAID' WHERE id = ?
```

1. Aggregates are never stored as single relational rows with mutable columns.
2. An Aggregate is reconstituted by applying its event stream sequentially from version `0` to version `N`.
3. To optimize reconstitution of aggregates with thousands of events, periodic **Snapshots** are saved every $K$ events (e.g., every 100 events), allowing playback to start from the snapshot.
4. Concurrency is handled by an integer `version` or sequence number in the event stream table. If two concurrent requests try to append version `3` to stream `order-123`, a unique constraint violation halts the second request.

---

## 4. Syntax & Usage

### 1. Modeling Commands and Events with Java 21 Records

```java
package com.example.cqrs.api;

import java.math.BigDecimal;
import java.time.Instant;
import java.util.UUID;

// 1. COMMANDS (Imperative, intent to mutate)
public sealed interface OrderCommand {
    UUID orderId();

    record CreateOrder(UUID orderId, UUID customerId, BigDecimal amount) implements OrderCommand {}
    record ShipOrder(UUID orderId, String trackingNumber) implements OrderCommand {}
    record CancelOrder(UUID orderId, String reason) implements OrderCommand {}
}

// 2. EVENTS (Past tense, immutable facts)
public sealed interface OrderEvent {
    UUID orderId();
    Instant occurredOn();

    record OrderCreated(UUID orderId, UUID customerId, BigDecimal amount, Instant occurredOn) implements OrderEvent {}
    record OrderShipped(UUID orderId, String trackingNumber, Instant occurredOn) implements OrderEvent {}
    record OrderCancelled(UUID orderId, String reason, Instant occurredOn) implements OrderEvent {}
}
```

---

## 5. Practical Examples

### Example 1: Pure Event-Sourced Aggregate Root

```java
package com.example.cqrs.domain;

import com.example.cqrs.api.OrderEvent;
import com.example.cqrs.api.OrderEvent.*;

import java.math.BigDecimal;
import java.time.Instant;
import java.util.ArrayList;
import java.util.List;
import java.util.UUID;

public class OrderAggregate {

    private UUID id;
    private String status;
    private BigDecimal amount;
    private int version = 0;

    // Uncommitted changes to be written to Event Store
    private final List<OrderEvent> uncommittedEvents = new ArrayList<>();

    public OrderAggregate() {}

    // Factory method processing command
    public static OrderAggregate create(UUID orderId, UUID customerId, BigDecimal amount) {
        OrderAggregate aggregate = new OrderAggregate();
        aggregate.applyChange(new OrderCreated(orderId, customerId, amount, Instant.now()));
        return aggregate;
    }

    public void ship(String trackingNumber) {
        if (!"CREATED".equals(this.status)) {
            throw new IllegalStateException("Cannot ship order in state: " + this.status);
        }
        applyChange(new OrderShipped(this.id, trackingNumber, Instant.now()));
    }

    private void applyChange(OrderEvent event) {
        mutate(event);
        uncommittedEvents.add(event);
    }

    // State mutator handling event replay
    public void mutate(OrderEvent event) {
        switch (event) {
            case OrderCreated e -> {
                this.id = e.orderId();
                this.amount = e.amount();
                this.status = "CREATED";
            }
            case OrderShipped e -> this.status = "SHIPPED";
            case OrderCancelled e -> this.status = "CANCELLED";
        }
        this.version++;
    }

    public UUID getId() { return id; }
    public int getVersion() { return version; }
    public List<OrderEvent> getUncommittedEvents() { return uncommittedEvents; }
    public void markChangesAsCommitted() { uncommittedEvents.clear(); }
}
```

### Example 2: Lightweight PostgreSQL Event Store using Spring `JdbcClient`

```java
package com.example.cqrs.infrastructure;

import com.example.cqrs.api.OrderEvent;
import com.fasterxml.jackson.databind.ObjectMapper;
import org.springframework.jdbc.core.simple.JdbcClient;
import org.springframework.stereotype.Repository;
import org.springframework.transaction.annotation.Transactional;

import java.util.List;
import java.util.UUID;

@Repository
public class PostgresEventStore {

    private final JdbcClient jdbcClient;
    private final ObjectMapper objectMapper;

    public PostgresEventStore(JdbcClient jdbcClient, ObjectMapper objectMapper) {
        this.jdbcClient = jdbcClient;
        this.objectMapper = objectMapper;
    }

    @Transactional
    public void appendEvents(UUID aggregateId, int expectedVersion, List<OrderEvent> events) {
        String sql = """
            INSERT INTO event_stream (aggregate_id, version, event_type, payload, occurred_on)
            VALUES (:aggId, :version, :type, :payload::jsonb, :occurredOn)
            """;

        int currentVersion = expectedVersion;
        for (OrderEvent event : events) {
            currentVersion++;
            try {
                String jsonPayload = objectMapper.writeValueAsString(event);
                jdbcClient.sql(sql)
                    .param("aggId", aggregateId)
                    .param("version", currentVersion)
                    .param("type", event.getClass().getSimpleName())
                    .param("payload", jsonPayload)
                    .param("occurredOn", event.occurredOn())
                    .update();
            } catch (Exception ex) {
                throw new RuntimeException("Optimistic lock failure or serialization error in event store", ex);
            }
        }
    }
}
```

### Example 3: Asynchronous Projection Engine Updating Read Model

```java
package com.example.cqrs.projection;

import com.example.cqrs.api.OrderEvent.OrderCreated;
import com.example.cqrs.api.OrderEvent.OrderShipped;
import org.springframework.jdbc.core.simple.JdbcClient;
import org.springframework.modulith.events.ApplicationModuleListener;
import org.springframework.stereotype.Component;

@Component
public class OrderSummaryProjection {

    private final JdbcClient jdbcClient;

    public OrderSummaryProjection(JdbcClient jdbcClient) {
        this.jdbcClient = jdbcClient;
    }

    @ApplicationModuleListener
    public void on(OrderCreated event) {
        jdbcClient.sql("""
            INSERT INTO order_summary_views (order_id, customer_id, total_amount, status, last_updated)
            VALUES (:id, :customerId, :amount, 'CREATED', :time)
            """)
            .param("id", event.orderId())
            .param("customerId", event.customerId())
            .param("amount", event.amount())
            .param("time", event.occurredOn())
            .update();
    }

    @ApplicationModuleListener
    public void on(OrderShipped event) {
        jdbcClient.sql("""
            UPDATE order_summary_views 
            SET status = 'SHIPPED', tracking_number = :tracking, last_updated = :time 
            WHERE order_id = :id
            """)
            .param("tracking", event.trackingNumber())
            .param("time", event.occurredOn())
            .param("id", event.orderId())
            .update();
    }
}
```

### Example 4: Anti-Pattern (Querying the Event Store Directly for Reporting) vs Clean CQRS

#### The Anti-Pattern: Aggregating Over Raw Events on Every Query
```java
// BAD: Attempting to calculate reporting dashboards by replaying all raw events on every GET request!
@GetMapping("/reports/revenue")
public BigDecimal calculateTotalRevenue() {
    // Reading 5 million event JSON rows into JVM memory to calculate sum
    List<EventRecord> events = eventStore.findAllByType("OrderCreated");
    return events.stream()
        .map(e -> deserialize(e.getPayload()).getAmount())
        .reduce(BigDecimal.ZERO, BigDecimal::add); // Catastrophic OOM & CPU spike!
}
```

#### Production Fix: Pre-computed Read Projection
```java
// GOOD: Instant O(1) query served from pre-aggregated projection view
@GetMapping("/reports/revenue")
public BigDecimal calculateTotalRevenueFast() {
    return jdbcClient.sql("SELECT total_revenue FROM daily_revenue_projections WHERE date = CURRENT_DATE")
        .query(BigDecimal.class)
        .single();
}
```

### Example 5: High-Speed Read-Side Controller with Caching

```java
package com.example.cqrs.web;

import org.springframework.http.ResponseEntity;
import org.springframework.jdbc.core.simple.JdbcClient;
import org.springframework.web.bind.annotation.*;

import java.util.UUID;

public record OrderSummaryDto(UUID orderId, String status, String trackingNumber) {}

@RestController
@RequestMapping("/api/v1/orders")
public class OrderQueryController {

    private final JdbcClient jdbcClient;

    public OrderQueryController(JdbcClient jdbcClient) {
        this.jdbcClient = jdbcClient;
    }

    @GetMapping("/{id}")
    public ResponseEntity<OrderSummaryDto> getOrder(@PathVariable UUID id) {
        return jdbcClient.sql("SELECT order_id, status, tracking_number FROM order_summary_views WHERE order_id = :id")
            .param("id", id)
            .query(OrderSummaryDto.class)
            .optional()
            .map(ResponseEntity::ok)
            .orElseGet(() -> ResponseEntity.notFound().build());
    }
}
```

---

## 6. Best Practices

1. **Keep Events Strictly Business-Focused and Immutable**: An event should capture business intent (`CustomerMovedAddress`), never technical database mutations (`CustomerTableUpdated`).
2. **Version All Event Schemas**: Plan for schema evolution. When adding fields, provide default values or write upcasters to migrate version 1 events to version 2 transparently.
3. **Use Idempotent Projection Handlers**: Because event distribution via Kafka or Outbox may deliver duplicate messages, write projectors using SQL `UPSERT` (`INSERT ... ON CONFLICT DO UPDATE`) to ensure idempotency.
4. **Take Periodic Snapshots**: For long-lived aggregates (e.g., bank accounts with 10,000 transactions), save aggregate snapshots every 100 events to prevent linear playback latency.
5. **Separate Read and Write Schemas**: Store events in an append-only table (`event_stream`), and maintain read models in dedicated projection tables or Elasticsearch indices.
6. **Handle Eventual Consistency in UI Design**: Inform users that updates are being processed (`Status: SUBMITTING`) or use polling/WebSockets/SSE to notify clients once the read projection catches up.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| **Updating Events in the Event Store** | Violates the foundational rule of Event Sourcing (immutability) and invalidates audit logs. | Never issue `UPDATE` or `DELETE` on the event store. Record compensating events (`OrderCancelled`, `PaymentRefunded`). |
| **Assuming Instant Read Consistency** | The UI redirects to the read model before the projector runs, showing stale data or 404 Not Found. | Return `202 Accepted` with correlation ID, or return the write command result directly. |
| **Omitting Optimistic Concurrency Checks** | Two concurrent threads append events to the same stream, creating corrupted aggregate state. | Enforce a unique database constraint on `(aggregate_id, version)` in the event table. |
| **Applying CQRS to Simple CRUD Apps** | Tremendous overhead of commands, events, projectors, and eventual consistency with zero business benefit. | Stick to classic Layered 3-tier architecture for standard CRUD. |
| **Non-Deterministic Event Handlers** | Calling `Instant.now()` or `UUID.randomUUID()` inside the aggregate's `mutate()` method causes state to diverge upon replay. | Pass timestamps and random values inside the Event payload itself. |
| **Re-inventing Axon or EventStoreDB for Complex Systems** | Writing custom projection engines, replay managers, and snapshot stores from scratch leads to edge-case bugs. | Consider mature frameworks like Axon Framework or EventStoreDB for complex enterprise ES systems. |

---

## 8. Performance & Security Notes

### Performance
- **Append-Only Write Throughput**: The Event Store performs strictly append-only `INSERT` operations. This maximizes database I/O performance by avoiding B-Tree index rebalancing, page locks, and row update overhead.
- **Asymmetric Scaling**: The read projection database can be scaled independently using read-replicas, Redis caches, or Elasticsearch clusters without affecting write throughput.

### Security & Compliance
- **Cryptographic Audit Trail**: Because events are append-only and immutable, Event Sourcing provides an inherent, tamper-evident audit log required by financial (SOX, PCI-DSS) and healthcare (HIPAA) compliance.
- **Right to Be Forgotten (GDPR)**: Deleting personal data in an immutable event store is a challenge. **Crypto-Shredding Pattern**: Encrypt sensitive PII in the event payload with a per-user encryption key. When the user exercises GDPR deletion, destroy their encryption key, rendering the historical event payload permanently unreadable.

---

## 9. Comparison / When to Use What

| Architectural Dimension | Traditional CRUD / JPA | Command Query Responsibility Segregation (CQRS) | CQRS + Event Sourcing (ES) |
|---|---|---|---|
| **State Storage** | Current mutable state row | Separate Read & Write tables | Append-only event stream log |
| **Historical Audit** | None (unless separate audit table) | None (unless explicitly audited) | 100% Complete Built-In History |
| **Consistency** | Strong / Immediate ACID | Eventual or Immediate | Eventual Consistency on Reads |
| **Complexity** | Low | Moderate | High |
| **Time Travel / Debugging**| Impossible (past state is lost) | Difficult | Replay events to any point in time |
| **Best Used For** | Standard CRUD, internal tools | High-scale reads vs writes | Fintech, logistics, complex domains |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>1. How does Event Sourcing handle optimistic concurrency control during concurrent writes?</strong></summary>

Each aggregate stream maintains an integer `version`.
When a command is received:
1. The aggregate is loaded at current version $V$ (e.g., $V = 5$).
2. Business rules execute, generating a new event.
3. The event store executes an append operation asserting `expectedVersion = 5`.
4. If another transaction appended an event in the interim (advancing the stream to version 6), the append operation fails with an optimistic locking conflict (enforced by a database unique constraint on `(aggregate_id, version)`).
5. The command handler catches the exception, reloads the new stream, and retries the command.
</details>

<details>
<summary><strong>2. What is "Eventual Consistency" in CQRS and how can the client UI accommodate it?</strong></summary>

In CQRS, writing to the Command side and projecting to the Read side are decoupled. The Read model updates asynchronously, creating a window (typically 5–50 milliseconds) where reads reflect stale data.
**UI Mitigation Strategies**:
1. **Optimistic UI Update**: The front-end immediately displays the expected state without waiting for the query API.
2. **Command Result Return**: The Command API endpoint returns the newly generated entity ID and initial state directly.
3. **SSE / WebSocket Notification**: The backend pushes an update to the browser once the projection completes.
4. **Polling with Version / ETag**: The client polls the read endpoint asserting `If-None-Match` until the expected version appears.
</details>

<details>
<summary><strong>3. How do you solve the GDPR "Right to be Forgotten" in an immutable Event Store?</strong></summary>

Under GDPR, users can demand complete deletion of their personal data. However, modifying or deleting rows in an append-only event store breaks aggregate stream replay and cryptographic hashes.
**Solution: Crypto-Shredding**:
- Every customer has an individual cryptographic key stored in a secure Key Management Service (AWS KMS, HashiCorp Vault).
- When appending events containing PII (name, email, SSN), the sensitive fields are encrypted with the customer's key.
- To fulfill a GDPR deletion request, delete the customer's encryption key.
- The historical events remain intact in the stream, but the encrypted PII is permanently mathematically unrecoverable.
</details>

<details>
<summary><strong>4. What is Aggregate Snapshotting and when should it be used?</strong></summary>

Reconstituting an aggregate with 50,000 historical events requires deserializing and applying 50,000 event objects, causing latency and high CPU usage.
**Snapshotting**:
- Every $N$ events (e.g., every 100 events), the aggregate's current state is serialized and saved to a `snapshots` table.
- When loading the aggregate, the system fetches the latest snapshot (e.g., at version 500) and replays only the events that occurred after version 500.
</details>

<details>
<summary><strong>5. Can CQRS be implemented without Event Sourcing?</strong></summary>

Yes, absolutely. CQRS simply states that the read and write models are separated.
A very common pattern is **CQRS with State Persistence**:
- The write model uses Spring Data JPA updating standard relational tables.
- Changes are published via database triggers or an Outbox table to an Elasticsearch or Redis instance for read queries.
- Event Sourcing is an optional storage mechanism that naturally complements CQRS, but is not required.
</details>

<details>
<summary><strong>6. What is Event Upcasting in Event Sourcing?</strong></summary>

As business software evolves, event payloads change (e.g., renaming a field or changing a data type).
Because historical events in the database are immutable and cannot be altered in-place:
- An **Upcaster** is an in-memory transformation filter that intercepts old event versions during deserialization and transforms them into the latest event schema before passing them to the aggregate or projector.
</details>

<details>
<summary><strong>7. Why must event handler methods (`apply()` / `mutate()`) be pure and deterministic?</strong></summary>

The method that mutates aggregate state based on an event will be executed repeatedly across the application lifetime during event replay.
If the method calls `Instant.now()`, fetches external HTTP data, or uses `Math.random()`, the aggregate's reconstituted state will vary unpredictably each time it is loaded from the event store.
All non-deterministic values must be computed beforehand in the command handler and stored inside the event payload.
</details>

<details>
<summary><strong>8. How does Spring Modulith support CQRS read-model projections?</strong></summary>

Spring Modulith provides `@ApplicationModuleListener`:
- It automatically integrates with Spring's event publication registry.
- It guarantees that the event publication and the primary write transaction are atomically bound.
- If the projection update fails, Spring Modulith automatically retries the projection handler without losing the event.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Model a Sealed Domain Event Hierarchy</strong></summary>

**Objective**: Use Java 21 `sealed` interfaces to create a closed, pattern-matchable event hierarchy for a user account.

**Solution**:
```java
public sealed interface AccountEvent permits AccountOpened, MoneyDeposited, AccountClosed {
    UUID accountId();
    Instant timestamp();
}

public record AccountOpened(UUID accountId, String ownerEmail, Instant timestamp) implements AccountEvent {}
public record MoneyDeposited(UUID accountId, BigDecimal amount, Instant timestamp) implements AccountEvent {}
public record AccountClosed(UUID accountId, String reason, Instant timestamp) implements AccountEvent {}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Write a Pattern-Matching Event Replayer</strong></summary>

**Objective**: Write a method that updates bank balance using Java 21 pattern matching over sealed events.

**Solution**:
```java
public BigDecimal calculateBalance(List<AccountEvent> history) {
    BigDecimal balance = BigDecimal.ZERO;
    for (AccountEvent event : history) {
        balance = switch (event) {
            case AccountOpened e -> BigDecimal.ZERO;
            case MoneyDeposited e -> balance.add(e.amount());
            case AccountClosed e -> BigDecimal.ZERO;
        };
    }
    return balance;
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Implement an Event Store Table Schema in SQL</strong></summary>

**Objective**: Write the PostgreSQL DDL for a production-ready event store table with optimistic concurrency control.

**Solution**:
```sql
CREATE TABLE IF NOT EXISTS event_stream (
    id BIGSERIAL PRIMARY KEY,
    aggregate_id UUID NOT NULL,
    version INT NOT NULL,
    event_type VARCHAR(255) NOT NULL,
    payload JSONB NOT NULL,
    occurred_on TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    CONSTRAINT uq_aggregate_version UNIQUE (aggregate_id, version)
);

CREATE INDEX idx_event_stream_agg_id ON event_stream (aggregate_id, version ASC);
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Build an Idempotent Projection Upsert</strong></summary>

**Objective**: Write an idempotent PostgreSQL projection update using `JdbcClient` and `ON CONFLICT`.

**Solution**:
```java
@Component
public class CustomerBalanceProjection {

    private final JdbcClient jdbcClient;

    public CustomerBalanceProjection(JdbcClient jdbcClient) {
        this.jdbcClient = jdbcClient;
    }

    public void updateBalance(UUID customerId, BigDecimal newBalance) {
        String sql = """
            INSERT INTO customer_balance_views (customer_id, balance, updated_at)
            VALUES (:id, :balance, NOW())
            ON CONFLICT (customer_id) 
            DO UPDATE SET balance = EXCLUDED.balance, updated_at = NOW()
            """;

        jdbcClient.sql(sql)
            .param("id", customerId)
            .param("balance", newBalance)
            .update();
    }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Implement Snapshotting for an Event-Sourced Aggregate</strong></summary>

**Objective**: Write the loading logic for an aggregate that checks for the latest snapshot before loading remaining events.

**Solution**:
```java
public OrderAggregate load(UUID orderId) {
    // 1. Fetch latest snapshot if present
    Optional<OrderSnapshot> snapshot = snapshotStore.findLatest(orderId);
    
    OrderAggregate aggregate = new OrderAggregate();
    int fromVersion = 0;

    if (snapshot.isPresent()) {
        aggregate.restoreFromSnapshot(snapshot.get());
        fromVersion = snapshot.get().version();
    }

    // 2. Fetch events occurred AFTER snapshot version
    List<OrderEvent> recentEvents = eventStore.findEvents(orderId, fromVersion);
    for (OrderEvent event : recentEvents) {
        aggregate.mutate(event);
    }

    return aggregate;
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Build an In-Memory CQRS Bus with Command and Query Dispatchers</strong></summary>

**Objective**: Implement a generic, type-safe Command and Query dispatcher bus in Spring Boot using Java generics and Spring's `ApplicationContext`.

**Solution**:
```java
public interface CommandHandler<C, R> {
    R handle(C command);
}

@Component
public class CommandBus {
    private final ApplicationContext context;

    public CommandBus(ApplicationContext context) {
        this.context = context;
    }

    @SuppressWarnings("unchecked")
    public <C, R> R dispatch(C command) {
        // Resolve bean implementing CommandHandler for this specific command class
        ResolvableType type = ResolvableType.forClassWithGenerics(CommandHandler.class, command.getClass(), Object.class);
        String[] beanNames = context.getBeanNamesForType(type);
        if (beanNames.length == 0) {
            throw new IllegalArgumentException("No handler found for command: " + command.getClass());
        }
        CommandHandler<C, R> handler = (CommandHandler<C, R>) context.getBean(beanNames[0]);
        return handler.handle(command);
    }
}
```
</details>

---

## 12. Summary & Cheat Sheet

```
+----------------------------------------------------------------------------------------------------+
| CQRS & EVENT SOURCING QUICK REFERENCE                                                              |
+----------------------------------------------------------------------------------------------------+
| Concept             | Characteristics                           | Practical Guideline              |
+---------------------+-------------------------------------------+----------------------------------+
| Command             | Imperative intent (CreateOrder); can fail | Validated against invariants     |
+---------------------+-------------------------------------------+----------------------------------+
| Event               | Past fact (OrderCreated); immutable       | Stored in append-only log        |
+---------------------+-------------------------------------------+----------------------------------+
| Event Store         | Append-only table; unique(agg_id, version)| No UPDATE or DELETE statements   |
+---------------------+-------------------------------------------+----------------------------------+
| Query / Projection  | Denormalized read view (SQL, Redis, ES)   | Updated asynchronously via events|
+---------------------+-------------------------------------------+----------------------------------+
| Optimistic Locking  | Enforced via expectedVersion check        | Protects aggregate consistency   |
+---------------------+-------------------------------------------+----------------------------------+
| Snapshotting        | State checkpoint saved every N events     | Prevents long event replays      |
+---------------------+-------------------------------------------+----------------------------------+
| Crypto-Shredding    | Encrypt PII with per-user key             | Enables GDPR deletion compliance |
+----------------------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Microsoft Patterns & Practices: CQRS Journey](https://learn.microsoft.com/en-us/previous-versions/msp-n-p/jj554200(v=pandp.10))
- [Event Store Documentation & Event Sourcing Basics](https://www.eventstore.com/event-sourcing)
- [Axon Framework: Java CQRS & Event Sourcing Framework](https://axoniq.io/products/axon-framework)
- [Martin Fowler on CQRS](https://martinfowler.com/bliki/CQRS.html)
- [Greg Young's CQRS and Event Sourcing Class](https://www.youtube.com/watch?v=LDW0QWie21s)
