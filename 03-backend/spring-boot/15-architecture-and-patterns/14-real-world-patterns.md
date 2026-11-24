# Real-World Enterprise Resilience and Data Patterns in Spring Boot

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.x |
| **Tested With** | PostgreSQL, Redis (Redisson), Kafka, ShedLock, Resilience4j |

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
Building enterprise backend services on distributed cloud infrastructure (Kubernetes, AWS, multi-node databases) forces developers to confront hostile real-world conditions: networks partition, duplicate HTTP requests arrive due to automated client retries, asynchronous events are delivered multiple times by message brokers (at-least-once delivery), scheduled background jobs execute simultaneously on 10 replica pods, and cascading failures bring down entire microservice clusters. Simple in-memory locks and naive database transactions cannot solve these distributed systems problems.

### The Evolution
Over decades of distributed systems engineering, five foundational enterprise patterns emerged to solve these exact challenges:
1. **The Transactional Outbox Pattern**: Guarantees atomic consistency between local database updates and published message broker events without distributed 2-Phase Commit (2PC).
2. **Distributed Locks (ShedLock & Redlock)**: Coordinates single-execution scheduling and critical sections across horizontally autoscaled pods.
3. **Idempotency Keys**: Guarantees that duplicate HTTP requests or duplicate message events produce the exact same outcome without charging cards twice or creating duplicate records.
4. **Soft Delete**: Preserves historical data integrity and regulatory auditability while hiding records from active application views.
5. **The SAGA Pattern**: Manages distributed long-running business transactions across independent microservice databases via orchestrated or choreographed compensating actions.

### Production Relevance
Mastering these patterns is mandatory for building resilient e-commerce checkouts, financial payment gateways, high-throughput event streaming systems, and mission-critical cloud-native services in Spring Boot.

---

## 2. Core Concepts

### Top Enterprise Resilience Patterns Topology

```
+-----------------------------------------------------------------------------------------+
|                                REAL-WORLD RESILIENCE PATTERNS                           |
+-----------------------------------------------------------------------------------------+
|                                                                                         |
|  1. TRANSACTIONAL OUTBOX PATTERN (Atomic DB + Event Publish)                            |
|     +-------------------------------------------------------------------------------+   |
|     | Local ACID DB Transaction:                                                    |   |
|     |   1. INSERT INTO orders VALUES (...);                                         |   |
|     |   2. INSERT INTO outbox_events (event_type, payload) VALUES (...);            |   |
|     +-------------------------------------------------------------------------------+   |
|            |                                                                            |
|            v (Debezium CDC / Polling Publisher Engine)                                  |
|     [ Apache Kafka Broker: "orders-topic" ] (Guaranteed At-Least-Once Delivery)         |
|                                                                                         |
|  2. IDEMPOTENT CONSUMER / API GATE (Duplicate Request Protection)                      |
|     Client Request with Header: "Idempotency-Key: e82f-48b2-..."                        |
|            |                                                                            |
|            v                                                                            |
|     [ Redis / Database Check: Has this key been processed in the last 24h? ]             |
|            +---> (YES) -> Return cached response immediately (Bypass business logic)     |
|            +---> (NO)  -> Lock key, process order, cache result, unlock key             |
|                                                                                         |
|  3. DISTRIBUTED LOCKS (ShedLock / Redis Redlock)                                        |
|     K8s Pod 1, Pod 2, Pod 3 all trigger @Scheduled(cron = "0 0 * * * *")                |
|            |                                                                            |
|            v                                                                            |
|     [ Central Lock Table / Redis ]: Only ONE pod acquires the lock for 10 minutes       |
|                                                                                         |
|  4. SOFT DELETE ARCHITECTURE                                                            |
|     UPDATE customer SET deleted_at = NOW() WHERE id = 101;                              |
|     - All SELECT queries automatically filtered via @SQLRestriction("deleted_at IS NULL")|
|                                                                                         |
+-----------------------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

### Deep Dive: The Transactional Outbox Pattern Lifecycle

```
[ Application Service ]
           |
           | 1. @Transactional begins
           v
[ PostgreSQL Database ]
   - Updates Business Table: INSERT INTO orders VALUES (order_101, $150.00);
   - Inserts Outbox Row:     INSERT INTO outbox_table (id, payload, status) 
                                       VALUES (uuid, '{"orderId":101}', 'PENDING');
   - Commits Transaction (Both records commit ATOMICALLY!)
           |
           +---------------------------------------------+
           |                                             |
           v (Method A: CDC via Debezium)                v (Method B: Spring Modulith / Poller)
   [ PostgreSQL WAL (Write-Ahead Log) ]        [ Scheduled Background Poller ]
           |                                             |
           v                                             v
   [ Debezium Connector ]                      [ Relay Service Worker ]
           |                                             |
           +---------------------+-----------------------+
                                 |
                                 v
                     [ Apache Kafka Event Topic ]
```

1. **Dual-Write Problem**: If an application writes to a database and then publishes to Kafka, what happens if Kafka is down or the JVM crashes between the two calls? Data inconsistency occurs.
2. **Outbox Guarantee**: By saving the event payload into an `outbox` table in the *exact same local database transaction* as the entity, database ACID properties guarantee that an event is saved if and only if the entity is saved.
3. **Relay**: An independent process (Debezium Change Data Capture or a Spring Modulith background poller) reads from the outbox table and streams the events to Kafka with guaranteed at-least-once delivery.

---

## 4. Syntax & Usage

### 1. Distributed Scheduling Lock with ShedLock

Add dependencies: `shedlock-spring` and `shedlock-provider-jdbc-template`.

```java
package com.example.pattern.shedlock;

import net.javacrumbs.shedlock.spring.annotation.EnableSchedulerLock;
import net.javacrumbs.shedlock.spring.annotation.SchedulerLock;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.context.annotation.Configuration;
import org.springframework.scheduling.annotation.EnableScheduling;
import org.springframework.scheduling.annotation.Scheduled;
import org.springframework.stereotype.Component;

@Configuration
@EnableScheduling
@EnableSchedulerLock(defaultLockAtMostFor = "10m")
public class ShedLockConfig {}

@Component
public class DistributedNightlyBillingTask {

    private static final Logger log = LoggerFactory.getLogger(DistributedNightlyBillingTask.class);

    // Guaranteed to run on exactly ONE pod in the cluster, even with 20 replica pods running!
    @Scheduled(cron = "0 0 2 * * *")
    @SchedulerLock(
        name = "DistributedNightlyBillingTask_run",
        lockAtLeastFor = "30s", // Prevents another pod from running if execution finishes in 5s
        lockAtMostFor = "15m"   // Prevents deadlock if the pod crashes mid-execution
    )
    public void runBilling() {
        log.info("Executing nightly billing run on thread: {}", Thread.currentThread());
        // Run billing calculation...
    }
}
```

---

## 5. Practical Examples

### Example 1: Transactional Outbox Pattern Implementation

```java
package com.example.pattern.outbox;

import com.fasterxml.jackson.databind.ObjectMapper;
import org.springframework.jdbc.core.simple.JdbcClient;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.time.Instant;
import java.util.UUID;

public record OrderEntity(UUID id, UUID customerId, double amount) {}
public record OrderPlacedEvent(UUID orderId, UUID customerId, double amount, Instant timestamp) {}

@Service
public class OrderOutboxService {

    private final JdbcClient jdbcClient;
    private final ObjectMapper objectMapper;

    public OrderOutboxService(JdbcClient jdbcClient, ObjectMapper objectMapper) {
        this.jdbcClient = jdbcClient;
        this.objectMapper = objectMapper;
    }

    @Transactional // Guarantees both DB write and Outbox insertion commit together atomically
    public UUID createOrder(UUID customerId, double amount) {
        UUID orderId = UUID.randomUUID();

        // 1. Insert domain entity into orders table
        jdbcClient.sql("INSERT INTO orders (id, customer_id, amount) VALUES (:id, :cust, :amt)")
                .param("id", orderId)
                .param("cust", customerId)
                .param("amt", amount)
                .update();

        // 2. Insert event into outbox table
        OrderPlacedEvent event = new OrderPlacedEvent(orderId, customerId, amount, Instant.now());
        try {
            String payload = objectMapper.writeValueAsString(event);
            jdbcClient.sql("""
                INSERT INTO outbox_table (id, aggregate_type, aggregate_id, event_type, payload, status, created_at)
                VALUES (:id, 'Order', :aggId, 'OrderPlacedEvent', :payload::jsonb, 'PENDING', NOW())
                """)
                .param("id", UUID.randomUUID())
                .param("aggId", orderId.toString())
                .param("payload", payload)
                .update();
        } catch (Exception ex) {
            throw new RuntimeException("Outbox serialization failure", ex);
        }

        return orderId;
    }
}
```

### Example 2: API Idempotency Gate with Redis and Redisson

```java
package com.example.pattern.idempotency;

import org.redisson.api.RLock;
import org.redisson.api.RMap;
import org.redisson.api.RedissonClient;
import org.springframework.stereotype.Service;

import java.time.Duration;
import java.util.concurrent.TimeUnit;

@Service
public class IdempotentPaymentService {

    private final RedissonClient redisson;

    public IdempotentPaymentService(RedissonClient redisson) {
        this.redisson = redisson;
    }

    public PaymentResponse processPayment(String idempotencyKey, PaymentRequest request) {
        RMap<String, PaymentResponse> completedResponses = redisson.getMap("idempotency:responses");
        
        // 1. Check if this exact request key was already processed
        PaymentResponse cached = completedResponses.get(idempotencyKey);
        if (cached != null) {
            return cached; // Return identical previous response without re-charging card!
        }

        // 2. Acquire distributed lock for this key to prevent concurrent race conditions
        RLock lock = redisson.getLock("idempotency:lock:" + idempotencyKey);
        try {
            boolean acquired = lock.tryLock(5, 30, TimeUnit.SECONDS);
            if (!acquired) {
                throw new IllegalStateException("Payment request is already being processed concurrently.");
            }

            // Re-check cache under lock
            if (completedResponses.containsKey(idempotencyKey)) {
                return completedResponses.get(idempotencyKey);
            }

            // 3. Execute actual payment gateway charge
            PaymentResponse response = executeRealPaymentCharge(request);

            // 4. Cache response in Redis for 24 hours
            completedResponses.put(idempotencyKey, response);
            completedResponses.expire(Duration.ofHours(24));

            return response;
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            throw new RuntimeException(e);
        } finally {
            if (lock.isHeldByCurrentThread()) {
                lock.unlock();
            }
        }
    }

    private PaymentResponse executeRealPaymentCharge(PaymentRequest req) {
        return new PaymentResponse("TXN_" + System.currentTimeMillis(), "SUCCESS");
    }
}
```

### Example 3: Soft Delete Architecture with Hibernate 6 `@SQLRestriction`

In Hibernate 6, `@Where` is deprecated in favor of `@SQLRestriction`:

```java
package com.example.pattern.softdelete;

import jakarta.persistence.*;
import org.hibernate.annotations.SQLDelete;
import org.hibernate.annotations.SQLRestriction;

import java.time.Instant;
import java.util.UUID;

@Entity
@Table(name = "customers")
// Intercepts repository.delete() and executes an UPDATE instead of a physical SQL DELETE
@SQLDelete(sql = "UPDATE customers SET deleted_at = NOW() WHERE id = ?")
// Automatically filters all SELECT queries to return only non-deleted records
@SQLRestriction("deleted_at IS NULL")
public class Customer {

    @Id
    @GeneratedValue(strategy = GenerationType.UUID)
    private UUID id;

    private String name;

    @Column(name = "deleted_at")
    private Instant deletedAt;

    public void restore() {
        this.deletedAt = null; // Enables un-deleting records!
    }

    public UUID getId() { return id; }
    public String getName() { return name; }
    public boolean isDeleted() { return deletedAt != null; }
}
```

### Example 4: Anti-Pattern (Dual-Write Failure) vs Outbox Solution

#### The Anti-Pattern: Unsynchronized Dual-Write
```java
// DANGEROUS ANTI-PATTERN: Prone to silent data corruption
@Service
public class DangerousOrderService {
    @Autowired private OrderRepository orderRepo;
    @Autowired private KafkaTemplate<String, Object> kafkaTemplate;

    @Transactional
    public void createOrder(Order order) {
        orderRepo.save(order); // Database write succeeds

        // CATASTROPHIC FAILURE SCENARIO:
        // Kafka network timeout or broker outage occurs!
        // The Kafka exception rolls back the database transaction.
        // OR: Kafka succeeds, but database commit fails.
        // Result: Kafka has an event for an order that does not exist in the DB!
        kafkaTemplate.send("orders-topic", order.getId(), new OrderPlacedEvent(order));
    }
}
```

#### Production Fix: Spring Modulith Event Publication Registry
```java
// GOOD: Automatically persisted to outbox table within the same DB transaction!
@Service
public class CleanOrderService {
    private final OrderRepository orderRepo;
    private final ApplicationEventPublisher events;

    public CleanOrderService(OrderRepository orderRepo, ApplicationEventPublisher events) {
        this.orderRepo = orderRepo;
        this.events = events;
    }

    @Transactional
    public void createOrder(Order order) {
        orderRepo.save(order);
        // Spring Modulith records event in DB outbox table atomically with order!
        events.publishEvent(new OrderPlacedEvent(order));
    }
}
```

### Example 5: Orchestrated SAGA Pattern with Compensating Transactions

```java
package com.example.pattern.saga;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;

import java.util.UUID;

@Service
public class OrderSagaOrchestrator {

    private static final Logger log = LoggerFactory.getLogger(OrderSagaOrchestrator.class);

    private final InventoryServiceClient inventoryClient;
    private final PaymentServiceClient paymentClient;
    private final ShippingServiceClient shippingClient;

    public OrderSagaOrchestrator(InventoryServiceClient inventoryClient, 
                                 PaymentServiceClient paymentClient, 
                                 ShippingServiceClient shippingClient) {
        this.inventoryClient = inventoryClient;
        this.paymentClient = paymentClient;
        this.shippingClient = shippingClient;
    }

    public boolean executeOrderSaga(UUID orderId, UUID customerId, double amount) {
        // Step 1: Forward Action - Reserve Inventory
        boolean inventoryReserved = inventoryClient.reserveInventory(orderId);
        if (!inventoryReserved) {
            log.error("SAGA ABORTED: Inventory reservation failed for order {}", orderId);
            return false;
        }

        // Step 2: Forward Action - Process Payment
        boolean paymentSuccess = paymentClient.chargeCustomer(orderId, customerId, amount);
        if (!paymentSuccess) {
            log.error("SAGA FAILED: Payment declined. Executing COMPENSATING transaction...");
            // Compensating Transaction: Roll back Step 1!
            inventoryClient.cancelInventoryReservation(orderId);
            return false;
        }

        // Step 3: Forward Action - Schedule Shipping
        try {
            shippingClient.scheduleDelivery(orderId);
            return true;
        } catch (Exception ex) {
            log.error("SAGA FAILED: Shipping failed. Executing cascading COMPENSATING actions...");
            // Compensating Transactions: Roll back Step 2, then Step 1!
            paymentClient.refundPayment(orderId);
            inventoryClient.cancelInventoryReservation(orderId);
            return false;
        }
    }
}
```

---

## 6. Best Practices

1. **Always Use Transactional Outbox for Message Broker Publishing**: Never execute `kafkaTemplate.send()` directly inside a database `@Transactional` method.
2. **Set Minimum and Maximum Lock Durations in ShedLock**: Always configure both `lockAtLeastFor` (guards against short tasks) and `lockAtMostFor` (guards against pod crashes).
3. **Pass Idempotency Keys in Client Mutation Requests**: Mandate an `Idempotency-Key` HTTP header for critical mutations (`POST /api/v1/payments`, `POST /api/v1/orders`).
4. **Use `@SQLRestriction` for Soft Delete in Hibernate 6**: Migrate away from deprecated `@Where` clauses.
5. **Enforce TTLs on Idempotency Records in Redis**: Set a 24-to-48 hour expiration (TTL) on cached idempotency keys to prevent unbounded memory growth.
6. **Design SAGA Compensating Actions to be Idempotent**: A compensating transaction (e.g., `refundPayment()`) may itself be retried multiple times during network splits; it must be completely safe to execute repeatedly.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| **Dual-Write Anti-Pattern** | Writing to PostgreSQL and Kafka sequentially; one fails while the other succeeds, causing data divergence. | Use the Transactional Outbox Pattern with Debezium or Spring Modulith. |
| **Omitting `lockAtLeastFor` in ShedLock** | A fast cron job finishes in 200ms; another pod wakes up and acquires the lock immediately, running it twice. | Always set `lockAtLeastFor = "15s"`. |
| **Omitting `lockAtMostFor` in ShedLock** | If a pod crashes while holding the lock, the lock is never released; the cron task is dead forever. | Set `lockAtMostFor` proportional to task execution time. |
| **Soft Delete Unique Constraint Collisions** | Unique index on `email` prevents creating a new user with the same email as a soft-deleted user. | Create partial unique indexes: `CREATE UNIQUE INDEX uq_email ON users (email) WHERE deleted_at IS NULL;` |
| **Caching Failed Responses Under Idempotency Keys**| Transient 503 errors get cached; client retries are permanently blocked from succeeding. | Only cache successful or non-retryable 4xx responses under the idempotency key. |
| **Distributed Locks without Watchdog Renewals** | Task takes longer than lock TTL; lock expires prematurely; another worker enters critical section concurrently. | Use Redisson's automatic lock watchdog or set generous safety margins. |

---

## 8. Performance & Security Notes

### Performance
- **Partial Indexes for Soft Delete**: Soft-deleted records bloat relational tables. In PostgreSQL, create partial indexes (`WHERE deleted_at IS NULL`) so active queries ignore deleted rows completely and execute with $O(\log N)$ speed.
- **Redisson Distributed Lock Latency**: Redisson acquires distributed locks via Redis PubSub rather than busy-polling loops, minimizing Redis CPU utilization.

### Security
- **Idempotency Key Scope Hijacking**: An attacker guesses another user's idempotency key and steals their cached transaction receipt.
  *Remedy*: Always namespace idempotency keys with the authenticated user ID: `key = "idempotency:" + authenticatedUserId + ":" + clientSuppliedKey`.

---

## 9. Comparison / When to Use What

| Pattern | Solves Which Problem? | Storage Mechanism | Implementation Tool |
|---|---|---|---|
| **Transactional Outbox**| Dual-write inconsistency | Relational DB + Kafka | Spring Modulith / Debezium |
| **ShedLock** | Multiple pods running same cron | DB table / Redis key | ShedLock library |
| **Redlock / Redisson** | Critical section concurrency | Multi-node Redis cluster | Redisson client |
| **Idempotency Key** | Duplicate requests & retries | Redis key with TTL / DB table| Custom Filter + Redis |
| **Soft Delete** | Audit compliance & data recovery| Relational DB timestamp col | Hibernate `@SQLDelete` / `@SQLRestriction` |
| **SAGA** | Distributed transactions | Multi-service orchestration | Orchestrator / Camunda / Event-Driven |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>1. What is the Dual-Write Problem and how does the Transactional Outbox Pattern solve it?</strong></summary>

The Dual-Write problem occurs when a microservice needs to update a database and publish an event to a message broker (Kafka/RabbitMQ) within the same business operation.
Because distributed transactions (2-Phase Commit / XA) are slow and unsupported by modern brokers:
- If the DB commit succeeds but Kafka publishing fails $\to$ Event is lost forever.
- If Kafka succeeds but DB commit fails $\to$ Ghost event published for non-existent data.
**The Outbox Pattern Solution**:
- The application inserts the domain entity and the event payload into an `outbox` table in the same local database transaction.
- Database ACID properties guarantee atomic commit.
- A separate Change Data Capture (CDC) engine (Debezium) reads the database write-ahead log (WAL) and publishes events to Kafka with zero loss.
</details>

<details>
<summary><strong>2. How does Redlock guarantee safety across a distributed Redis cluster?</strong></summary>

Developed by Salvatore Sanfilippo (creator of Redis), Redlock provides distributed lock safety without a single point of failure:
1. Connects to $N$ independent Redis master nodes (typically 5).
2. Attempts to acquire the lock in all $N$ instances sequentially using the same key and random value with a small timeout.
3. The lock is considered acquired if and only if the client acquired the lock in a **majority** of nodes ($\ge 3$ out of 5) within the validity time.
4. If acquisition fails, the client unlocks all instances immediately.
</details>

<details>
<summary><strong>3. What is the difference between Orchestrated SAGA and Choreographed SAGA?</strong></summary>

- **Orchestrated SAGA**: A centralized coordinator (orchestrator class or workflow engine) explicitly invokes each participant microservice via command/response. If a step fails, the orchestrator issues explicit compensating commands to earlier participants in reverse order. Easier to understand and monitor.
- **Choreographed SAGA**: Decentralized. Participants listen to domain events published by other services and execute local transactions independently. No central coordinator exists. Simpler for small workflows, but complex to visualize and debug for multi-step processes.
</details>

<details>
<summary><strong>4. How do you handle unique database constraints when implementing Soft Delete?</strong></summary>

If a table has a `UNIQUE(email)` constraint:
- User creates account with `alice@example.com`.
- User deletes account (`deleted_at = NOW()`).
- Another user attempts to register with `alice@example.com`.
- Database throws `UniqueConstraintViolationException` because the soft-deleted row still exists!
**Solution: Partial Unique Index (PostgreSQL)**:
Drop the standard unique constraint and create a partial index:
```sql
CREATE UNIQUE INDEX uq_user_email_active ON users (email) WHERE deleted_at IS NULL;
```
Now, multiple soft-deleted rows can share the same email, but active (`deleted_at IS NULL`) rows must remain globally unique.
</details>

<details>
<summary><strong>5. Why is `lockAtLeastFor` necessary in ShedLock?</strong></summary>

In cloud platforms, server clocks can experience minor clock drift (a few hundred milliseconds).
Furthermore, if a scheduled job executes very quickly (e.g., taking 50 milliseconds to check an empty queue):
- Pod 1 acquires the lock, completes the work in 50ms, and releases the lock.
- Pod 2 wakes up 100ms later on its own clock tick, sees the lock is free, and executes the exact same job again!
`lockAtLeastFor = "30s"` ensures that once acquired, no other pod can acquire the lock until at least 30 seconds have elapsed, preventing duplicate runs.
</details>

<details>
<summary><strong>6. How should an idempotent API handle a request that is currently in-progress?</strong></summary>

If Client Request A arrives with `Idempotency-Key: K1` and is currently executing:
When a duplicate Client Request B arrives with the same `K1`:
- It should **NOT** execute the business logic concurrently.
- It should either:
  1. Return HTTP `409 Conflict` with detail: `"Request with this idempotency key is currently in progress."`
  2. Or hold a short distributed lock waiting for Request A to finish, and return Request A's cached response once complete.
</details>

<details>
<summary><strong>7. What happens if a SAGA compensating transaction itself fails?</strong></summary>

Compensating transactions (e.g., refunding a customer or unreserving inventory) cannot simply give up if they encounter a transient network timeout.
**Resolution**:
1. **Automated Retries**: Compensating actions must be wrapped in exponential backoff retries until they succeed.
2. **Dead Letter Queue (DLQ)**: If all retries are exhausted, the event is routed to an operations DLQ.
3. **Manual Human Intervention**: SRE runbooks alert on-call engineers to manually reconcile the ledger.
</details>

<details>
<summary><strong>8. How does Spring Modulith implement the Transactional Outbox pattern out-of-the-box?</strong></summary>

Spring Modulith includes the **Event Publication Registry**:
- By adding `spring-modulith-starter-jpa` and `@EnablePersistentDomainEvents`:
- Any domain event published via `ApplicationEventPublisher` during an active `@Transactional` method is intercepted.
- Spring Modulith writes the serialized event into an `event_publication` table inside the *exact same database transaction*.
- Event publication to external listeners is managed with automatic retries upon failure.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Create ShedLock Database Lock Table</strong></summary>

**Objective**: Write the SQL DDL for the standard ShedLock coordination table in PostgreSQL.

**Solution**:
```sql
CREATE TABLE IF NOT EXISTS shedlock (
    name VARCHAR(64) NOT NULL PRIMARY KEY,
    lock_until TIMESTAMPTZ NOT NULL,
    locked_at TIMESTAMPTZ NOT NULL,
    locked_by VARCHAR(255) NOT NULL
);
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Configure Soft Delete Entity with Hibernate 6</strong></summary>

**Objective**: Create a `Product` entity that executes soft delete via `@SQLDelete` and `@SQLRestriction`.

**Solution**:
```java
@Entity
@Table(name = "products")
@SQLDelete(sql = "UPDATE products SET is_deleted = true WHERE id = ?")
@SQLRestriction("is_deleted = false")
public class Product {
    @Id @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;
    private String name;
    
    @Column(name = "is_deleted", nullable = false)
    private boolean isDeleted = false;

    public Long getId() { return id; }
    public String getName() { return name; }
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Implement an Idempotency Filter for REST Endpoints</strong></summary>

**Objective**: Write a Spring servlet filter that checks for an `X-Idempotency-Key` header and returns cached responses from Redis.

**Solution**:
```java
@Component
public class IdempotencyServletFilter extends OncePerRequestFilter {

    private final StringRedisTemplate redis;

    public IdempotencyServletFilter(StringRedisTemplate redis) {
        this.redis = redis;
    }

    @Override
    protected void doFilterInternal(HttpServletRequest request, HttpServletResponse response, FilterChain filterChain)
            throws ServletException, IOException {

        if (!"POST".equalsIgnoreCase(request.getMethod())) {
            filterChain.doFilter(request, response);
            return;
        }

        String key = request.getHeader("X-Idempotency-Key");
        if (key != null && !key.isBlank()) {
            String cachedBody = redis.opsForValue().get("idemp:" + key);
            if (cachedBody != null) {
                response.setContentType("application/json");
                response.setStatus(HttpServletResponse.SC_OK);
                response.getWriter().write(cachedBody);
                return;
            }
        }

        filterChain.doFilter(request, response);
    }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Implement Outbox Poller Worker</strong></summary>

**Objective**: Write a scheduled task that queries pending outbox rows, publishes them to Kafka, and marks them `SENT`.

**Solution**:
```java
@Component
public class OutboxRelayWorker {

    private final JdbcClient jdbcClient;
    private final KafkaTemplate<String, String> kafkaTemplate;

    public OutboxRelayWorker(JdbcClient jdbcClient, KafkaTemplate<String, String> kafkaTemplate) {
        this.jdbcClient = jdbcClient;
        this.kafkaTemplate = kafkaTemplate;
    }

    @Scheduled(fixedDelay = 2000)
    @Transactional
    public void processOutboxBatch() {
        // Fetch up to 50 pending events locking them for update
        List<OutboxRecord> records = jdbcClient.sql("""
            SELECT id, aggregate_id, event_type, payload 
            FROM outbox_table 
            WHERE status = 'PENDING' 
            ORDER BY created_at ASC 
            LIMIT 50 
            FOR UPDATE SKIP LOCKED
            """)
            .query(OutboxRecord.class)
            .list();

        for (OutboxRecord rec : records) {
            kafkaTemplate.send("domain-events", rec.aggregateId(), rec.payload());
            jdbcClient.sql("UPDATE outbox_table SET status = 'SENT', processed_at = NOW() WHERE id = :id")
                .param("id", rec.id())
                .update();
        }
    }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Distributed Mutex using Redisson with Watchdog</strong></summary>

**Objective**: Use Redisson to execute a critical stock reservation routine guaranteeing that no two pods sell the same seat concurrently.

**Solution**:
```java
@Service
public class ConcertTicketingService {

    private final RedissonClient redisson;
    private final TicketRepository ticketRepository;

    public ConcertTicketingService(RedissonClient redisson, TicketRepository ticketRepository) {
        this.redisson = redisson;
        this.ticketRepository = ticketRepository;
    }

    public boolean reserveSeat(String seatId, String customerId) {
        RLock lock = redisson.getLock("seat_lock:" + seatId);
        try {
            // Wait up to 3s to acquire; hold lock with automatic watchdog lease renewal
            if (lock.tryLock(3, TimeUnit.SECONDS)) {
                try {
                    Ticket ticket = ticketRepository.findBySeatId(seatId).orElseThrow();
                    if (ticket.isReserved()) {
                        return false;
                    }
                    ticket.setReservedBy(customerId);
                    ticketRepository.save(ticket);
                    return true;
                } finally {
                    lock.unlock();
                }
            }
            return false; // Could not acquire lock in 3s
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            return false;
        }
    }
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Integration Test Verifying ShedLock Distributed Coordination</strong></summary>

**Objective**: Write an integration test asserting that when two threads invoke a `@SchedulerLock` method simultaneously, only one succeeds.

**Solution**:
```java
@SpringBootTest
class ShedLockConcurrencyTest {

    @Autowired private DistributedNightlyBillingTask billingTask;

    @Test
    void assertLockPreventsSimultaneousExecution() throws Exception {
        ExecutorService executor = Executors.newFixedThreadPool(2);
        CountDownLatch latch = new CountDownLatch(2);
        AtomicInteger executedCount = new AtomicInteger(0);

        Runnable task = () -> {
            try {
                billingTask.runBilling();
                executedCount.incrementAndGet();
            } finally {
                latch.countDown();
            }
        };

        executor.submit(task);
        executor.submit(task);

        latch.await(5, TimeUnit.SECONDS);
        // Because of ShedLock, exactly ONE execution must succeed; the second is locked out!
        assertEquals(1, executedCount.get());
    }
}
```
</details>

---

## 12. Summary & Cheat Sheet

```
+----------------------------------------------------------------------------------------------------+
| REAL-WORLD RESILIENCE PATTERNS CHEAT SHEET                                                         |
+----------------------------------------------------------------------------------------------------+
| Pattern             | Solution / Framework                      | Primary Architectural Purpose    |
+---------------------+-------------------------------------------+----------------------------------+
| Transactional Outbox| Write event to outbox table inside local  | Guarantees atomic consistency    |
|                     | DB @Transactional; relay via Debezium/poll| between DB updates and Kafka     |
+---------------------+-------------------------------------------+----------------------------------+
| Distributed Lock    | ShedLock (for cron tasks);                | Prevents duplicate execution on  |
|                     | Redisson Redlock (for critical sections)  | multi-pod autoscaled clusters    |
+---------------------+-------------------------------------------+----------------------------------+
| Idempotency Key     | Redis check on 'X-Idempotency-Key' header;| Prevents duplicate charges &     |
|                     | cache 200 OK responses with 24h TTL       | replay attacks on client retries |
+---------------------+-------------------------------------------+----------------------------------+
| Soft Delete         | Hibernate 6 @SQLDelete & @SQLRestriction  | Retains audit history; partial   |
|                     | partial indexes (WHERE deleted_at IS NULL)| indexes prevent unique collision |
+---------------------+-------------------------------------------+----------------------------------+
| SAGA Pattern        | Orchestrator executing forward actions &  | Manages distributed transactions |
|                     | compensating transactions in reverse order| across microservice databases    |
+----------------------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Chris Richardson: Microservices Patterns (Transactional Outbox & SAGA)](https://microservices.io/patterns/data/transactional-outbox.html)
- [ShedLock GitHub Repository and Documentation](https://github.com/lukas-krecan/ShedLock)
- [Redisson: Distributed Java Locks and Data Structures on Redis](https://redisson.org/)
- [Martin Fowler on Sagas and Distributed Transactions](https://martinfowler.com/articles/patterns-of-distributed-systems/saga.html)
- [Stripe Engineering: Designing Robust and Idempotent APIs](https://stripe.com/blog/idempotency)
