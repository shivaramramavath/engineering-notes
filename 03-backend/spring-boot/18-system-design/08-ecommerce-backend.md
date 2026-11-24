# 08: Designing a High-Contention Flash Sale E-Commerce Backend

| Metadata Field | Value |
|---|---|
| **Topic** | High-Contention Inventory Reservation, Flash Sales & Distributed Orders |
| **Spring Boot Version** | 3.4.x |
| **Java Version** | 21 / 25 LTS |
| **Target Scale** | 100,000 Concurrent Buyers Competing for 1,000 Limited Items |
| **Key Middleware** | Redis Cluster (Atomic Lua Scripts), PostgreSQL (ACID Ledger), Apache Kafka, Debezium |

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

A "Flash Sale" scenario (e.g., Black Friday, concert ticket drop, sneaker release) represents one of the most extreme stress tests for an e-commerce backend: 100,000 concurrent shoppers hitting the checkout button within a 2-second window for only 1,000 physical inventory items.

Handling this in a traditional database with pessimistic locks (`SELECT ... FOR UPDATE`) or optimistic locks (`@Version`) causes catastrophic database thread pool exhaustion, deadlocks, and connection timeouts. To survive, the architecture must **decouple inventory reservation from database persistence**:
1. Reserve stock in sub-millisecond memory using **atomic Redis Lua scripts**.
2. Immediately reject the 99,000 unsuccessful requests at the Redis layer without touching the database.
3. Asynchronously persist the 1,000 successful orders into PostgreSQL using the **Transactional Outbox Pattern** and Kafka.

---

## 2. Core Concepts

### 2.1 Atomic Redis Lua Scripts
Redis executes Lua scripts atomically in a single-threaded event loop. By executing checking and stock decrement logic in a single script, race conditions and overselling are eliminated with zero database queries.

### 2.2 Temporary Stock Hold with Auto-Release (TTL)
When a user wins an item, a 15-minute hold is placed. If the user fails to complete payment within 15 minutes, the stock is automatically rolled back to the available pool via a scheduled job or Redis key-expiration listener.

---

## 3. Internals & Architecture

```
        [ 100,000 Concurrent Checkout Requests ]
                          │
                          ▼
       ┌─────────────────────────────────────┐
       │     API Gateway / Rate Limiter      │
       │   - 1 request / sec per User ID     │
       └──────────────────┬──────────────────┘
                          │
                          ▼
       ┌─────────────────────────────────────┐
       │   Spring Boot Flash Sale Service    │
       │   (Java 21 Virtual Threads)         │
       └──────────────────┬──────────────────┘
                          │
                          │ Execute Atomic Lua Script
                          ▼
       ┌─────────────────────────────────────┐
       │      Redis In-Memory Inventory      │
       │  Key: "item:stock:SKU-99" = 1000    │
       │  IF stock >= qty THEN               │
       │      stock = stock - qty            │
       │      RETURN SUCCESS                 │
       │  ELSE RETURN OUT_OF_STOCK           │
       └──────────────────┬──────────────────┘
                          │
         ┌────────────────┴────────────────┐
         ▼ (99,000 Rejections)             ▼ (1,000 Successes)
   [ Fast 409 Out of Stock ]     [ Enqueue to Kafka Order Stream ]
                                           │
                                           ▼
                                 ┌───────────────────┐
                                 │ Order Persistence │
                                 │ Worker Pods       │
                                 └─────────┬─────────┘
                                           │
                                           ▼
                                 ┌───────────────────┐
                                 │ PostgreSQL DB     │
                                 │ - orders table    │
                                 │ - inventory ledger│
                                 └───────────────────┘
```

---

## 4. Syntax & Practical Usage

### 4.1 Atomic Inventory Reservation Lua Script
```lua
local stockKey = KEYS[1]
local userHoldKey = KEYS[2]
local qty = tonumber(ARGV[1])
local userId = ARGV[2]

local currentStock = tonumber(redis.call('get', stockKey) or '0')

if currentStock >= qty then
    redis.call('decrby', stockKey, qty)
    redis.call('setex', userHoldKey, 900, qty) -- 15 min hold
    return 1 -- SUCCESS
else
    return 0 -- OUT_OF_STOCK
end
```

---

## 5. 5 Practical Examples

### Example 1: Spring Data Redis Lua Script Execution Service

```java
package com.example.ecommerce.service;

import org.springframework.core.io.ClassPathResource;
import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.data.redis.core.script.DefaultRedisScript;
import org.springframework.stereotype.Service;

import java.util.Collections;
import java.util.List;

@Service
public class FlashSaleReservationService {

    private final StringRedisTemplate redisTemplate;
    private final DefaultRedisScript<Long> reservationScript;

    public FlashSaleReservationService(StringRedisTemplate redisTemplate) {
        this.redisTemplate = redisTemplate;
        this.reservationScript = new DefaultRedisScript<>();
        this.reservationScript.setLocation(new ClassPathResource("scripts/reserve_stock.lua"));
        this.reservationScript.setResultType(Long.class);
    }

    public boolean tryReserveStock(String sku, String userId, int quantity) {
        String stockKey = "flash:stock:" + sku;
        String userHoldKey = "flash:hold:" + sku + ":" + userId;

        List<String> keys = List.of(stockKey, userHoldKey);
        Long result = redisTemplate.execute(
                reservationScript,
                keys,
                String.valueOf(quantity),
                userId
        );

        return result != null && result == 1L;
    }
}
```

### Example 2: Flash Sale Checkout REST Controller

```java
package com.example.ecommerce.web;

import com.example.ecommerce.service.FlashSaleReservationService;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.kafka.core.KafkaTemplate;
import org.springframework.web.bind.annotation.*;

import java.util.Map;
import java.util.UUID;

@RestController
@RequestMapping("/api/v1/flash-sale")
public class FlashSaleController {

    private static final Logger log = LoggerFactory.getLogger(FlashSaleController.class);
    private final FlashSaleReservationService reservationService;
    private final KafkaTemplate<String, Object> kafkaTemplate;

    public FlashSaleController(FlashSaleReservationService reservationService, KafkaTemplate<String, Object> kafkaTemplate) {
        this.reservationService = reservationService;
        this.kafkaTemplate = kafkaTemplate;
    }

    public record FlashCheckoutRequest(String sku, String userId, int quantity) {}
    public record FlashCheckoutResponse(String orderId, String status, String message) {}

    @PostMapping("/checkout")
    public ResponseEntity<FlashCheckoutResponse> checkout(@RequestBody FlashCheckoutRequest request) {
        // Fast-path atomic check in Redis memory
        boolean reserved = reservationService.tryReserveStock(request.sku(), request.userId(), request.quantity());

        if (!reserved) {
            return ResponseEntity.status(HttpStatus.CONFLICT)
                    .body(new FlashCheckoutResponse(null, "OUT_OF_STOCK", "Item sold out or insufficient quantity"));
        }

        String orderId = UUID.randomUUID().toString();
        log.info("Stock reserved for user {} on SKU {}. Enqueueing order {}.", request.userId(), request.sku(), orderId);

        // Asynchronously enqueue confirmed order to Kafka for PostgreSQL persistence
        kafkaTemplate.send("flash-orders-confirmed", orderId, Map.of(
                "orderId", orderId,
                "sku", request.sku(),
                "userId", request.userId(),
                "quantity", request.quantity()
        ));

        return ResponseEntity.status(HttpStatus.ACCEPTED)
                .body(new FlashCheckoutResponse(orderId, "PENDING_PAYMENT", "Stock reserved for 15 minutes"));
    }
}
```

### Example 3: Double-Entry Financial Ledger Entity

```java
package com.example.ecommerce.domain;

import jakarta.persistence.*;
import java.math.BigDecimal;
import java.time.Instant;
import java.util.UUID;

@Entity
@Table(name = "financial_ledger_entries")
public class FinancialLedgerEntry {

    @Id
    @GeneratedValue(strategy = GenerationType.UUID)
    private UUID id;

    @Column(name = "transaction_id", nullable = false)
    private UUID transactionId;

    @Column(name = "account_id", nullable = false)
    private String accountId;

    @Enumerated(EnumType.STRING)
    @Column(nullable = false)
    private EntryType type; // DEBIT or CREDIT

    @Column(nullable = false, precision = 12, scale = 2)
    private BigDecimal amount;

    @Column(name = "created_at", nullable = false, updatable = false)
    private Instant createdAt = Instant.now();

    public enum EntryType { DEBIT, CREDIT }

    public FinancialLedgerEntry() {}

    public FinancialLedgerEntry(UUID transactionId, String accountId, EntryType type, BigDecimal amount) {
        this.transactionId = transactionId;
        this.accountId = accountId;
        this.type = type;
        this.amount = amount;
    }

    public UUID getId() { return id; }
    public BigDecimal getAmount() { return amount; }
    public EntryType getType() { return type; }
}
```

### Example 4: Asynchronous Order Persistence Worker

```java
package com.example.ecommerce.worker;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.jdbc.core.simple.JdbcClient;
import org.springframework.kafka.annotation.KafkaListener;
import org.springframework.stereotype.Component;
import org.springframework.transaction.annotation.Transactional;

import java.util.Map;

@Component
public class FlashOrderPersistenceWorker {

    private static final Logger log = LoggerFactory.getLogger(FlashOrderPersistenceWorker.class);
    private final JdbcClient jdbcClient;

    public FlashOrderPersistenceWorker(JdbcClient jdbcClient) {
        this.jdbcClient = jdbcClient;
    }

    @KafkaListener(topics = "flash-orders-confirmed", groupId = "order-persister-group")
    @Transactional
    public void persistOrder(Map<String, Object> orderPayload) {
        String orderId = (String) orderPayload.get("orderId");
        String sku = (String) orderPayload.get("sku");
        String userId = (String) orderPayload.get("userId");
        int quantity = (Integer) orderPayload.get("quantity");

        log.info("Persisting flash order {} to database ledger...", orderId);

        jdbcClient.sql("""
                INSERT INTO orders (id, user_id, sku, quantity, status)
                VALUES (?, ?, ?, ?, 'PENDING_PAYMENT')
                ON CONFLICT (id) DO NOTHING
                """)
                .params(orderId, userId, sku, quantity)
                .update();
    }
}
```

### Example 5: 15-Minute Unpaid Stock Rollback Task

```java
package com.example.ecommerce.scheduler;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.jdbc.core.simple.JdbcClient;
import org.springframework.scheduling.annotation.Scheduled;
import org.springframework.stereotype.Component;
import org.springframework.transaction.annotation.Transactional;

import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.List;

@Component
public class ExpiredStockRollbackScheduler {

    private static final Logger log = LoggerFactory.getLogger(ExpiredStockRollbackScheduler.class);
    private final JdbcClient jdbcClient;
    private final StringRedisTemplate redisTemplate;

    public ExpiredStockRollbackScheduler(JdbcClient jdbcClient, StringRedisTemplate redisTemplate) {
        this.jdbcClient = jdbcClient;
        this.redisTemplate = redisTemplate;
    }

    public record ExpiredOrder(String id, String sku, int quantity) {}

    @Scheduled(fixedRate = 60000) // Runs every minute
    @Transactional
    public void releaseExpiredReservations() {
        Instant cutoff = Instant.now().minus(15, ChronoUnit.MINUTES);

        List<ExpiredOrder> expiredOrders = jdbcClient.sql("""
                SELECT id, sku, quantity FROM orders 
                WHERE status = 'PENDING_PAYMENT' AND created_at < ?
                """)
                .param(cutoff)
                .query(ExpiredOrder.class)
                .list();

        for (ExpiredOrder order : expiredOrders) {
            log.info("Order {} expired unpaid. Restoring {} units of SKU {} to Redis.", order.id(), order.quantity(), order.sku());
            redisTemplate.opsForValue().increment("flash:stock:" + order.sku(), order.quantity());

            jdbcClient.sql("UPDATE orders SET status = 'CANCELLED_EXPIRED' WHERE id = ?")
                    .param(order.id())
                    .update();
        }
    }
}
```

---

## 6. Best Practices

- **Never Let Checkout Requests Hit RDBMS Under Contention**: 100k database transactions competing for 1,000 rows will exhaust HikariCP and cause deadlocks. Isolate inventory in Redis.
- **Pre-Warm Redis Before Sale Launches**: Load available stock numbers into Redis 10 minutes before the sale goes live.
- **Implement Virtual Waiting Rooms (Queue-It)**: When traffic spikes beyond capacity, place users in an encrypted FIFO token queue at the edge (Cloudflare / Nginx).
- **Use Double-Entry Bookkeeping**: In the financial database, every transaction must balance debits and credits:
  $$\sum \text{Debits} - \sum \text{Credits} = 0$$
- **Cap Max Quantity per User**: Enforce `quantity <= 2` in the Lua script to block scalpers and bot networks.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
|---|---|---|
| **`SELECT FOR UPDATE` on Hot Stock Row** | 100k threads block on a single database row lock, exhausting connection pools and crashing the DB. | Decrement stock in Redis via atomic Lua scripts. |
| **Separating Check & Decrement in Redis** | `GET stock` followed by `DECRBY stock` is a race condition that leads to overselling. | Execute both atomically inside a single Lua script. |
| **Synchronous Payment Gateway Calls** | Waiting 3 seconds for Stripe during peak checkout freezes application threads. | Return HTTP 202 with payment session; process webhooks asynchronously. |
| **No Auto-Release for Abandoned Carts** | Users reserve items but never pay, locking inventory away from real buyers. | Set a 15-minute hold TTL and run a rollback reconciler job. |
| **Single Primary DB for Reads and Writes** | Flash sale traffic crashes the database, taking down normal site browsing. | Route catalog reads to CDN and read replicas; isolate flash sale writes in Kafka. |
| **Floating Point Currency Storage** | Using `FLOAT` or `DOUBLE` for monetary values leads to IEEE-754 precision rounding errors. | Always use `NUMERIC(12, 2)` or integer cents in database schemas and Java `BigDecimal`. |

---

## 8. Performance & Security Considerations

### 8.1 Bot & Scalper Mitigation
Require CAPTCHA verification (Cloudflare Turnstile) before invoking `/checkout`. Require signed user tokens tied to authenticated device IDs.

### 8.2 Redis High Availability
Deploy Redis Cluster with sentinel failover and append-only file (`appendonly yes`) enabled to prevent inventory loss on pod restart.

---

## 9. Comparison / Decision Matrix

| Mechanism | Max Throughput | Concurrency Safety | Database Impact |
|---|---|---|---|
| **JPA `@Version` Optimistic Lock** | $< 500\text{ QPS}$ | Fails with high `OptimisticLockException` | High DB CPU |
| **Pessimistic `PESSIMISTIC_WRITE`** | $< 200\text{ QPS}$ | Safe (Locks row) | Catastrophic deadlock risk |
| **Redis Lua In-Memory Atomic Reservation** | $100,000+\text{ QPS}$ | Completely thread-safe | Zero database load during spike |

---

## 10. Real-World Interview Q&A

<details>
<summary><strong>1. Why does traditional database locking fail during flash sales?</strong></summary>

When 100,000 requests try to acquire a lock on the same database row (`SELECT FOR UPDATE`), the database engine wastes all its CPU managing lock queues and context switches, starving the connection pool and leading to cascading timeouts.
</details>

<details>
<summary><strong>2. How does an atomic Redis Lua script prevent overselling?</strong></summary>

Redis processes commands sequentially on its main thread. A Lua script executes as a single uninterruptible atomic unit; no other command can execute between checking stock and decrementing stock, eliminating race conditions.
</details>

<details>
<summary><strong>3. How do you handle users who reserve an item but never pay?</strong></summary>

The reservation carries a 15-minute expiration timestamp. A background scheduled job or Redis key-expiration listener queries unpaid reservations older than 15 minutes, cancels the order, and increments the Redis stock back up.
</details>

<details>
<summary><strong>4. How do you ensure that confirmed orders are not lost if the database is temporarily slow?</strong></summary>

After Redis decrements stock, the event is immediately published to Apache Kafka. Kafka durably persists the order on disk across partitions. Consumer workers drain Kafka and persist orders into PostgreSQL at a controlled, sustainable rate.
</details>

<details>
<summary><strong>5. What is double-entry bookkeeping, and why is it used in e-commerce?</strong></summary>

Every financial movement is recorded as both an offsetting debit and credit in immutable ledger tables. This guarantees that funds cannot be created or destroyed due to bugs or partial failures.
</details>

<details>
<summary><strong>6. How do you prevent bots from draining inventory in 100 milliseconds?</strong></summary>

Enforce token bucket rate limiting (1 checkout per user/IP), require proof-of-work or Cloudflare Turnstile CAPTCHA, and mandate verified user account ages ($> 30\text{ days}$).
</details>

<details>
<summary><strong>7. How do you handle read traffic on product pages during the sale?</strong></summary>

Offload 95% of product catalog traffic to CloudFront/Fastly CDN edge caches with 5-second `stale-while-revalidate` TTLs.
</details>

<details>
<summary><strong>8. What happens if the Redis server crashes during the flash sale?</strong></summary>

Deploy Redis in a Cluster with synchronous in-memory replication to replicas and persistence enabled (AOF with `appendfsync everysec`). If a master fails, Redis Sentinel automatically promotes a replica within 2 seconds.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Write an Atomic Decrement Lua Script</strong></summary>

**Prompt**: Write a Lua script decrementing stock only if current stock $\ge$ requested quantity.

**Solution**:
Inspect Section 4.1: retrieve stock via `redis.call('get', KEYS[1])`, verify `currentStock >= qty`, and call `redis.call('decrby', KEYS[1], qty)`.
</details>

<details>
<summary><strong>Exercise 2: Implement Double-Entry Balance Validator</strong></summary>

**Prompt**: Write a Java service method verifying that the sum of debits equals the sum of credits for a transaction.

**Solution**:
```java
public boolean validateLedger(List<FinancialLedgerEntry> entries) {
    BigDecimal debits = entries.stream()
            .filter(e -> e.getType() == EntryType.DEBIT)
            .map(FinancialLedgerEntry::getAmount)
            .reduce(BigDecimal.ZERO, BigDecimal::add);
    BigDecimal credits = entries.stream()
            .filter(e -> e.getType() == EntryType.CREDIT)
            .map(FinancialLedgerEntry::getAmount)
            .reduce(BigDecimal.ZERO, BigDecimal::add);
    return debits.compareTo(credits) == 0;
}
```
</details>

<details>
<summary><strong>Exercise 3: Simulate 100 Threads Competing for 10 Items</strong></summary>

**Prompt**: Write a JUnit test with Testcontainers Redis where 100 threads attempt to reserve 1 item each from a stock of 10. Verify exactly 10 succeed and 90 fail.

**Solution**:
Seed Redis key with "10", fire 100 threads with `CountDownLatch`, assert `successCount.get() == 10` and `failCount.get() == 90`.
</details>

<details>
<summary><strong>Exercise 4: Implement Redis Stock Rollback</strong></summary>

**Prompt**: Write a method releasing reserved stock when a payment fails.

**Solution**:
```java
public void rollbackStock(String sku, int quantity) {
    redisTemplate.opsForValue().increment("flash:stock:" + sku, quantity);
}
```
</details>

<details>
<summary><strong>Exercise 5: Configure Kafka Order Persister Consumer</strong></summary>

**Prompt**: Configure a `@KafkaListener` consumer with concurrency 5 for draining the flash orders topic.

**Solution**:
```java
@KafkaListener(topics = "flash-orders-confirmed", groupId = "order-persister", concurrency = "5")
public void handleOrder(Map<String, Object> order) {
    // Write to PostgreSQL
}
```
</details>

<details>
<summary><strong>Exercise 6: Test Idempotent Order Insertion in PostgreSQL</strong></summary>

**Prompt**: Write a SQL query using `ON CONFLICT (id) DO NOTHING` to guarantee that duplicate Kafka events do not create duplicate orders.

**Solution**:
Inspect Example 4: `INSERT INTO orders (id, ...) VALUES (...) ON CONFLICT (id) DO NOTHING`.
</details>

---

## 12. Summary & Quick Reference Cheatsheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                   FLASH SALE ARCHITECTURE CHEATSHEET                   │
├────────────────────────────────────────────────────────────────────────┤
│ 1. Zero DB Contention: Check & decrement stock in Redis memory via Lua │
│ 2. Instant Feedback: 99% losers get immediate HTTP 409 Out of Stock   │
│ 3. Async Persistence: 1% winners queued to Kafka -> drained to Postgres│
│ 4. 15-Minute Expiry: Rollback unpaid stock via scheduled reconciler    │
│ 5. Financial Safety: Double-entry ledger (Debits == Credits)           │
│ 6. Bot Protection: CAPTCHA, 1 req/sec rate limit per user ID           │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading & Official Resources

- [Redis Programmability with Lua Scripts](https://redis.io/docs/interact/programmability/eval-intro/)
- [Martin Kleppmann: Designing Data-Intensive Applications](https://dataintensive.net/)
- [E-Commerce Flash Sale System Design (Alex Xu)](https://bytebytego.com/)
- [Spring for Apache Kafka Reference](https://docs.spring.io/spring-kafka/reference/)
