# 08: Messaging & Event-Driven Systems Technical Interview Q&A

| Metadata Field | Value |
|---|---|
| **Topic** | Apache Kafka, RabbitMQ, Consumer Group Rebalancing & Outbox Pattern |
| **Spring Boot Version** | 3.4.x |
| **Spring Kafka / AMQP** | 3.3.x / 3.2.x |
| **Java Version** | 21 / 25 LTS |
| **Target Level** | Senior / Lead / Staff Backend Engineer |
| **Key Focus Areas** | Kafka Consumer Groups, Manual Offset Commits, Transactional Outbox, Dead Letter Topics |

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

Event-driven architectures decouple microservices, enable horizontal scalability, and provide natural buffering against traffic spikes. In enterprise Java systems, **Apache Kafka** (for distributed event streaming and log replay) and **RabbitMQ** (for complex message routing and task queues) are the dominant backbones.

However, messaging introduces distributed concurrency edge cases:
- How to ensure **atomic consistency between database writes and message publishing** without fragile distributed transactions (Two-Phase Commit).
- How to configure **Kafka consumer offsets** (`AckMode.MANUAL_IMMEDIATE`) to prevent data loss or duplicate processing.
- How to handle **poison pill messages** using Dead Letter Topics (DLT) without stalling consumer partitions.
- How Kafka manages **Consumer Group Rebalancing** and partition reassignments.

---

## 2. Core Concepts

### 2.1 The Transactional Outbox Pattern
Writing to PostgreSQL and publishing to Kafka in the same method suffers from the dual-write problem: if the database commits but the network drops before Kafka acknowledges, the event is lost.
- **Outbox Solution**: Store the event in an `outbox_events` table inside the exact same database transaction as the business entity.
- A background worker (or Debezium CDC) reads uncommitted outbox rows and publishes them to Kafka with guaranteed at-least-once delivery.

### 2.2 Kafka Consumer Offset Commit Modes
- `RECORD`: Commit offset after each individual record is processed. High overhead.
- `BATCH` (Default): Commit offset after the entire polled batch finishes.
- `MANUAL_IMMEDIATE`: The application code invokes `Acknowledgment.acknowledge()` immediately after database persistence succeeds.

### 2.3 Kafka Partition Rebalancing
When a consumer joins, crashes, or takes longer than `max.poll.interval.ms` (default: 5 minutes) to process a batch, the Kafka group coordinator initiates a **Rebalance**. During eager rebalances, all consumers stop processing ("Stop-the-world"). Modern Kafka uses **Cooperative Sticky Assignor**, allowing unaffected consumers to continue working uninterrupted.

---

## 3. Internals & Architecture

```
 ┌────────────────────────────────────────────────────────┐
 │           Spring Service Transaction (Local DB)        │
 │  1. INSERT INTO orders (...)                           │
 │  2. INSERT INTO outbox_events (event_payload...)       │
 │  3. COMMIT Transaction Atomically                      │
 └───────────────────────────┬────────────────────────────┘
                             │
                             ▼
 ┌────────────────────────────────────────────────────────┐
 │       Debezium CDC / Outbox Poller Worker              │
 │  - Reads outbox_events table                           │
 │  - Publishes to Kafka Topic: "orders-topic"            │
 └───────────────────────────┬────────────────────────────┘
                             │
                             ▼
 ┌────────────────────────────────────────────────────────┐
 │                  Apache Kafka Cluster                  │
 │  Topic: "orders-topic" (12 Partitions)                 │
 └───────────────────────────┬────────────────────────────┘
                             │
                             ▼
 ┌────────────────────────────────────────────────────────┐
 │         Spring Kafka Consumer Group                    │
 │  1. Poll batch of records                              │
 │  2. Process business logic (Idempotent check)          │
 │  3. acknowledgment.acknowledge() (Manual Commit)       │
 └───────────────────────────┬────────────────────────────┘
                             │ (On Error after 3 Retries)
                             ▼
 ┌────────────────────────────────────────────────────────┐
 │       Dead Letter Topic (DLT): "orders-topic.DLT"      │
 └────────────────────────────────────────────────────────┘
```

---

## 4. Syntax & Practical Usage

### 4.1 Manual Ack Kafka Listener Syntax
```java
@KafkaListener(topics = "orders-topic", groupId = "order-consumers")
public void onOrderMessage(ConsumerRecord<String, OrderDto> record, Acknowledgment ack) {
    // Process business logic...
    ack.acknowledge(); // Commits offset immediately
}
```

---

## 5. 5 Practical Examples

### Example 1: Transactional Outbox Pattern with Spring Modulith

```java
package com.example.messaging.order;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.context.ApplicationEventPublisher;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.math.BigDecimal;
import java.util.UUID;

@Service
public class OrderService {

    private static final Logger log = LoggerFactory.getLogger(OrderService.class);
    private final OrderRepository orderRepository;
    private final ApplicationEventPublisher eventPublisher;

    public OrderService(OrderRepository orderRepository, ApplicationEventPublisher eventPublisher) {
        this.orderRepository = orderRepository;
        this.eventPublisher = eventPublisher;
    }

    public record OrderCreatedEvent(UUID orderId, BigDecimal amount) {}

    @Transactional
    public Order createOrder(String customerId, BigDecimal amount) {
        Order order = new Order(customerId, amount);
        Order saved = orderRepository.save(order);

        // Spring Modulith's Event Publication Registry intercepts this event
        // and persists it to the event_publication table in the SAME transaction!
        log.info("Publishing OrderCreatedEvent to Transactional Outbox...");
        eventPublisher.publishEvent(new OrderCreatedEvent(saved.getId(), saved.getAmount()));

        return saved;
    }
}
```

### Example 2: Manual Ack Kafka Consumer with Idempotency

```java
package com.example.messaging.consumer;

import org.apache.kafka.clients.consumer.ConsumerRecord;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.kafka.annotation.KafkaListener;
import org.springframework.kafka.support.Acknowledgment;
import org.springframework.stereotype.Component;

import java.time.Duration;

@Component
public class IdempotentKafkaConsumer {

    private static final Logger log = LoggerFactory.getLogger(IdempotentKafkaConsumer.class);
    private final StringRedisTemplate redisTemplate;

    public IdempotentKafkaConsumer(StringRedisTemplate redisTemplate) {
        this.redisTemplate = redisTemplate;
    }

    @KafkaListener(
            topics = "orders-stream",
            groupId = "fulfillment-group",
            containerFactory = "manualAckKafkaListenerContainerFactory"
    )
    public void consumeOrder(ConsumerRecord<String, String> record, Acknowledgment ack) {
        String eventId = record.key();
        log.info("Received Kafka event with key: {}", eventId);

        // Idempotency check via Redis
        Boolean isFirstTime = redisTemplate.opsForValue()
                .setIfAbsent("seen:event:" + eventId, "1", Duration.ofHours(48));

        if (Boolean.FALSE.equals(isFirstTime)) {
            log.warn("Duplicate event {} detected. Acknowledging and skipping execution.", eventId);
            ack.acknowledge();
            return;
        }

        try {
            // Execute business logic (e.g. fulfill shipment)
            processShipment(record.value());

            // Acknowledge offset only after successful processing
            ack.acknowledge();
            log.info("Event {} processed and offset committed.", eventId);

        } catch (Exception ex) {
            log.error("Failed to process event {}: {}", eventId, ex.getMessage());
            // Do not ack! Let error handler or retry backoff manage it
            throw ex;
        }
    }

    private void processShipment(String payload) {
        // Business logic
    }
}
```

### Example 3: Dead Letter Publishing Recoverer Configuration

```java
package com.example.messaging.config;

import org.apache.kafka.common.TopicPartition;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.kafka.core.KafkaTemplate;
import org.springframework.kafka.listener.DeadLetterPublishingRecoverer;
import org.springframework.kafka.listener.DefaultErrorHandler;
import org.springframework.util.backoff.ExponentialBackOff;

@Configuration
public class KafkaDeadLetterConfig {

    @Bean
    public DefaultErrorHandler kafkaErrorHandler(KafkaTemplate<String, Object> kafkaTemplate) {
        // Route failed records to <topic>.DLT after 3 retries
        DeadLetterPublishingRecoverer recoverer = new DeadLetterPublishingRecoverer(
                kafkaTemplate,
                (record, ex) -> new TopicPartition(record.topic() + ".DLT", record.partition())
        );

        ExponentialBackOff backOff = new ExponentialBackOff(1000L, 2.0);
        backOff.setMaxInterval(8000L);
        backOff.setMaxElapsedTime(20000L); // Max 20 seconds retry duration

        DefaultErrorHandler errorHandler = new DefaultErrorHandler(recoverer, backOff);
        // Exclude unrecoverable validation exceptions from retries
        errorHandler.addNotRetryableExceptions(IllegalArgumentException.class);
        return errorHandler;
    }
}
```

### Example 4: Container Factory with `MANUAL_IMMEDIATE` AckMode

```java
package com.example.messaging.config;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.kafka.config.ConcurrentKafkaListenerContainerFactory;
import org.springframework.kafka.core.ConsumerFactory;
import org.springframework.kafka.listener.ContainerProperties;

@Configuration
public class KafkaFactoryConfig {

    @Bean
    public ConcurrentKafkaListenerContainerFactory<String, Object> manualAckKafkaListenerContainerFactory(
            ConsumerFactory<String, Object> consumerFactory,
            DefaultErrorHandler errorHandler) {

        ConcurrentKafkaListenerContainerFactory<String, Object> factory =
                new ConcurrentKafkaListenerContainerFactory<>();
        factory.setConsumerFactory(consumerFactory);
        factory.getContainerProperties().setAckMode(ContainerProperties.AckMode.MANUAL_IMMEDIATE);
        factory.setCommonErrorHandler(errorHandler);
        factory.setConcurrency(3); // 3 worker threads per pod
        return factory;
    }
}
```

### Example 5: RabbitMQ DLX and Priority Queue Configuration

```java
package com.example.messaging.config;

import org.springframework.amqp.core.*;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

@Configuration
public class RabbitPriorityConfig {

    @Bean
    public Queue priorityQueue() {
        return QueueBuilder.durable("tasks.priority")
                .maxPriority(10) // Supports 0 to 10 priority weights
                .withArgument("x-dead-letter-exchange", "tasks.dlx")
                .withArgument("x-dead-letter-routing-key", "tasks.poison")
                .build();
    }
}
```

---

## 6. Best Practices

- **Never Attempt Dual-Writing Directly**: Always use the Transactional Outbox Pattern to guarantee that database changes and message events never diverge.
- **Use `MANUAL_IMMEDIATE` Offset AckMode**: Acknowledge offsets explicitly in application code only after local database transactions commit.
- **Implement Idempotent Consumers**: Because networks can fail before offset commits are recorded, consumers must anticipate duplicate message deliveries.
- **Use Cooperative Sticky Assignor**: Configure `partition.assignment.strategy: org.apache.kafka.clients.consumer.CooperativeStickyAssignor` to avoid stop-the-world partition rebalances.
- **Tune `max.poll.interval.ms`**: If long-running batch jobs exceed this threshold (default: 300,000ms), Kafka considers the consumer dead and triggers an unnecessary rebalance.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
|---|---|---|
| **Dual-Write Anti-Pattern** | Committing DB and then publishing to Kafka leads to lost messages if Kafka is unreachable. | Use the Transactional Outbox Pattern or Debezium CDC. |
| **Auto-Commit Enabled (`enable.auto.commit = true`)**| Commits offsets on a timer regardless of whether the record was processed successfully, causing data loss. | Set `enable.auto.commit = false` and use manual acknowledgements. |
| **Infinite Retry Loops on Poison Pills** | A malformed JSON payload crashes the consumer repeatedly, blocking all subsequent partition messages. | Use `DeadLetterPublishingRecoverer` with a max retry limit. |
| **Processing Longer than `max.poll.interval.ms`** | Kafka broker evicts the consumer, triggering continuous consumer group rebalances. | Reduce batch size (`max.poll.records`) or offload heavy processing to worker pools. |
| **Unkeyed Kafka Messages for Ordered Events** | Without a message key, messages are distributed round-robin across partitions, destroying event ordering. | Always set the entity ID (e.g., `orderId`) as the Kafka message key. |
| **Monolithic Single Partition** | A single partition restricts consumption to exactly one consumer thread, creating a throughput bottleneck. | Provision partitions based on target throughput (e.g. 12–24 partitions). |

---

## 8. Performance & Security Considerations

### 8.1 Partition Sizing
$$\text{Partitions} = \max\left(\frac{\text{Target Write QPS}}{\text{Producer Throughput}}, \frac{\text{Target Read QPS}}{\text{Consumer Throughput}}\right)$$
Kafka partitions should generally target between 10 MB/sec and 30 MB/sec throughput.

### 8.2 End-to-End Encryption
Enforce TLS encryption between Spring Boot producers/consumers and Kafka brokers (`security.protocol: SSL`), and authenticate via SASL/SCRAM or mTLS.

---

## 9. Comparison / Decision Matrix

| Dimension | Apache Kafka | RabbitMQ |
|---|---|---|
| **Paradigm** | Append-only distributed commit log | Smart message broker, dumb consumer |
| **Ordering** | Guaranteed per partition | Guaranteed per queue (single consumer) |
| **Replayability** | Yes (Retained on disk for days/weeks)| No (Deleted after ack) |
| **Message Routing** | Simple (Partition Key) | Advanced (Direct, Topic, Fanout, Headers) |
| **Backpressure** | Pull-based (Consumer controls rate) | Push-based (Prefetch controls rate) |

---

## 10. Real-World Interview Q&A

<details>
<summary><strong>1. What is the Dual-Write problem, and how does the Transactional Outbox pattern solve it?</strong></summary>

The Dual-Write problem occurs when an application tries to write to a database and publish to a message broker in the same method. If one succeeds and the other fails, data becomes permanently inconsistent. The Transactional Outbox pattern inserts an event record into an outbox database table within the same ACID transaction as the business entity. A separate process (or Debezium CDC) reads the outbox and publishes events to Kafka with guaranteed delivery.
</details>

<details>
<summary><strong>2. How does Kafka guarantee message ordering?</strong></summary>

Kafka guarantees total ordering strictly within an individual partition. By supplying a message key (e.g. `customerId`), Kafka hashes the key to ensure that all events for that specific entity always land in the exact same partition and are consumed sequentially by a single consumer thread.
</details>

<details>
<summary><strong>3. What triggers a Kafka Consumer Group Rebalance?</strong></summary>

A rebalance occurs when:
1. A new consumer joins the group.
2. An existing consumer crashes or disconnects.
3. A consumer fails to send heartbeats within `session.timeout.ms`.
4. A consumer takes longer than `max.poll.interval.ms` to process a polled batch of records.
</details>

<details>
<summary><strong>4. What is the difference between Eager Rebalancing and Cooperative Sticky Rebalancing?</strong></summary>

Eager rebalancing revokes all partition assignments across all consumers during a rebalance ("stop-the-world"). Cooperative Sticky rebalancing only revokes and reassigns partitions that actually need to move, allowing unaffected consumers to continue processing data without interruption.
</details>

<details>
<summary><strong>5. How do you handle poison pill messages in Spring Kafka?</strong></summary>

Configure a `DefaultErrorHandler` with a `DeadLetterPublishingRecoverer`. After a set number of retry attempts (e.g. 3 attempts with exponential backoff), the recoverer forwards the unprocessable message to a Dead Letter Topic (`<topic>.DLT`) and commits the offset so normal partition processing can resume.
</details>

<details>
<summary><strong>6. Why should you disable `enable.auto.commit` in production Kafka consumers?</strong></summary>

Auto-commit commits offsets periodically in the background regardless of whether the consumer has finished processing the business logic. If the consumer crashes halfway through processing a batch, uncommitted records are marked as committed, leading to permanent data loss.
</details>

<details>
<summary><strong>7. How does Spring Modulith help with the Outbox pattern?</strong></summary>

Spring Modulith includes a built-in Event Publication Registry. When a domain event is published via `ApplicationEventPublisher`, Spring Modulith automatically intercepts it and writes it into an `event_publication` database table in the active transaction, managing completion tracking and automatic retries upon restart.
</details>

<details>
<summary><strong>8. When would you choose RabbitMQ over Kafka?</strong></summary>

Choose RabbitMQ when you need complex routing rules (topic wildcards, headers exchange, fanout), granular per-message acknowledgements, priority queues, and request-reply (RPC) messaging patterns.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Configure Manual Ack Container Factory</strong></summary>

**Prompt**: Configure `ConcurrentKafkaListenerContainerFactory` with `AckMode.MANUAL_IMMEDIATE` and concurrency 3.

**Solution**:
Inspect Example 4: instantiate factory, set consumer factory, configure `setAckMode(AckMode.MANUAL_IMMEDIATE)`, and set concurrency to 3.
</details>

<details>
<summary><strong>Exercise 2: Implement Consumer with Idempotency Guard</strong></summary>

**Prompt**: Write a Kafka consumer method that checks Redis before processing to ensure no duplicate event IDs are processed twice.

**Solution**:
Inspect Example 2: call `redis.opsForValue().setIfAbsent("seen:event:" + key, ...)` and acknowledge immediately if false.
</details>

<details>
<summary><strong>Exercise 3: Configure Dead Letter Publishing Recoverer</strong></summary>

**Prompt**: Configure an error handler that retries 3 times with exponential backoff and publishes to topic `.DLT`.

**Solution**:
Inspect Example 3: define `DefaultErrorHandler` wrapping `DeadLetterPublishingRecoverer` and `ExponentialBackOff`.
</details>

<details>
<summary><strong>Exercise 4: Test Kafka Outbox Integration with Testcontainers</strong></summary>

**Prompt**: Write an integration test asserting that saving an order publishes an event to an embedded Kafka topic.

**Solution**:
Use Testcontainers Kafka and PostgreSQL. Call `orderService.createOrder()`. Use a test consumer to verify receipt of `OrderCreatedEvent` within 5 seconds.
</details>

<details>
<summary><strong>Exercise 5: Exclude Non-Retryable Exceptions from Kafka Error Handler</strong></summary>

**Prompt**: Configure `DefaultErrorHandler` to immediately route `JsonParseException` to the DLT without retrying.

**Solution**:
```java
errorHandler.addNotRetryableExceptions(JsonParseException.class);
```
</details>

<details>
<summary><strong>Exercise 6: Configure RabbitMQ Priority Queue</strong></summary>

**Prompt**: Declare a durable RabbitMQ queue with maximum priority 10.

**Solution**:
Inspect Example 5: `QueueBuilder.durable("tasks.priority").maxPriority(10).build()`.
</details>

---

## 12. Summary & Quick Reference Cheatsheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                   MESSAGING & KAFKA INTERVIEW CHEATSHEET               │
├────────────────────────────────────────────────────────────────────────┤
│ 1. Dual-Write Safety: Transactional Outbox Pattern via DB table/CDC    │
│ 2. Offsets: enable.auto.commit = false; use AckMode.MANUAL_IMMEDIATE   │
│ 3. Poison Pills: DeadLetterPublishingRecoverer with exponential backoff│
│ 4. Rebalancing: Use CooperativeStickyAssignor to avoid stop-the-world  │
│ 5. Ordering: Guaranteed strictly per partition using message keys      │
│ 6. Polling Timeout: Keep batch processing well below max.poll.interval │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading & Official Resources

- [Spring for Apache Kafka Reference Documentation](https://docs.spring.io/spring-kafka/reference/)
- [Spring Modulith Event Publication Registry](https://docs.spring.io/spring-modulith/reference/events.html)
- [Confluent: Kafka Consumer Group Rebalancing Guide](https://www.confluent.io/blog/cooperative-rebalancing-in-kafka-streams-consumer-ksqldb/)
- [The Transactional Outbox Pattern (Chris Richardson)](https://microservices.io/patterns/data/transactional-outbox.html)
