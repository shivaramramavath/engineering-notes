# Spring for Apache Kafka: KafkaTemplate, Listeners, and Concurrency

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | `spring-kafka:3.3.x`, `org.apache.kafka:kafka-clients:3.9.x` |

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
In distributed microservice architectures, synchronous HTTP REST communication introduces tight coupling, latency amplification across chained services, and single points of failure. If an order service must synchronously call payment, inventory, fraud, and notification services, any single service degradation cascades into total system failure. Traditional point-to-point message brokers (like standard JMS queues) struggle with multi-terabyte message persistence, millions of events per second, and allowing multiple independent consumer groups to replay identical message streams at their own pace.

### The Evolution
Apache Kafka fundamentally reimagined messaging as a **distributed, partitioned, append-only commit log**. Topics are partitioned across cluster nodes, enabling horizontal scalability and high throughput. **Spring for Apache Kafka (`spring-kafka`)** wraps the low-level `Producer` and `Consumer` APIs into high-level Spring idioms: `KafkaTemplate` for publishing, `@KafkaListener` for declarative consumption, automated error handling, manual offset acknowledgment, and transaction synchronization.

### Relevance in Modern Engineering
Kafka is the backbone of modern event-driven architectures (EDA), Event Sourcing, CQRS, and real-time streaming platforms. Spring Boot 3 provides non-blocking Producer futures (`CompletableFuture<SendResult>`), virtual thread execution for Kafka listeners, and native dead-letter topic (`.DLT`) routing.

---

## 2. Core Concepts

- **Topic & Partition**: A Topic is a logical category of messages; Partitions are ordered, immutable commit logs distributed across brokers. Partitions are the fundamental unit of parallelism.
- **Message Key & Ordering**: Messages with the same non-null key are hashed and guaranteed to land on the *same partition*, preserving strict FIFO ordering per key.
- **Consumer Group & Offsets**: Consumers collaborate within a Consumer Group. Each partition is assigned to exactly one consumer within a group. Offsets track consumer read progress.
- **`KafkaTemplate<K, V>`**: High-level thread-safe Spring template for publishing messages asynchronously or synchronously.
- **`@KafkaListener`**: Annotation designating a method as an event-driven listener for Kafka topics.
- **Acknowledgment Modes**:
  - `RECORD`: Commits offset after each record is processed.
  - `BATCH`: Commits offset after the entire polled batch is processed (default).
  - `MANUAL` / `MANUAL_IMMEDIATE`: Explicit user-controlled offset commit via `Acknowledgment.acknowledge()`.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        Spring Kafka Producer & Consumer Flow                      |
|                                                                                   |
| 1. Publishing via KafkaTemplate                                                   |
|    kafkaTemplate.send("orders-topic", orderId, orderRecord)                       |
|                   |                                                               |
|                   v                                                               |
| 2. Kafka Producer Pipeline                                                        |
|    +-----------------------------+     +-----------------------------+            |
|    | Serializer (String / JSON)  | --> | Partitioner (hash(key))     |            |
|    +-----------------------------+     +-----------------------------+            |
|                   |                                                               |
|                   v Appended to internal RecordAccumulator batch buffer           |
|    Sender I/O Thread flushes batch to Broker Partition (Leader)                   |
|                                                                                   |
| 3. Consumption via @KafkaListener                                                 |
|    KafkaMessageListenerContainer (Spawns worker thread per configured concurrency)|
|                   |                                                               |
|                   v                                                               |
| 4. Polling Loop: consumer.poll(Duration)                                          |
|    +-----------------------------+                                                |
|    | MessageListenerContainer    | ---> Deserializes ConsumerRecord               |
|    +-----------------------------+                                                |
|                   | Invokes @KafkaListener method                                 |
|                   v                                                               |
| 5. Acknowledgment & Offset Commit                                                 |
|    Success -> Commits offset to __consumer_offsets internal topic                 |
|    Failure -> ErrorHandler retries or routes to dead-letter topic (DLT)           |
+-----------------------------------------------------------------------------------+
```

On the producer side, `KafkaTemplate` delegates to the underlying `KafkaProducer`. The record passes through configured serializers and partitioners into an in-memory `RecordAccumulator`. A background sender thread dispatches batches across socket channels to cluster brokers. On the consumer side, `ConcurrentMessageListenerContainer` launches a pool of listener threads (`concurrency = N`). Each thread executes a `poll()` loop, invoking the annotated `@KafkaListener` method with deserialized records.

---

## 4. Syntax & Usage

### Maven Dependency
```xml
<dependency>
    <groupId>org.springframework.kafka</groupId>
    <artifactId>spring-kafka</artifactId>
</dependency>
```

### Essential Application Properties
```properties
# Kafka Cluster Configuration
spring.kafka.bootstrap-servers=localhost:9092

# Producer Config
spring.kafka.producer.key-serializer=org.apache.kafka.common.serialization.StringSerializer
spring.kafka.producer.value-serializer=org.springframework.kafka.support.serializer.JsonSerializer
spring.kafka.producer.acks=all
spring.kafka.producer.properties.enable.idempotence=true

# Consumer Config
spring.kafka.consumer.group-id=order-processing-group
spring.kafka.consumer.auto-offset-reset=earliest
spring.kafka.consumer.key-deserializer=org.apache.kafka.common.serialization.StringDeserializer
spring.kafka.consumer.value-deserializer=org.springframework.kafka.support.serializer.JsonDeserializer
spring.kafka.consumer.properties.spring.json.trusted.packages=com.example.platform.*
spring.kafka.listener.ack-mode=manual_immediate
```

---

## 5. Practical Examples

### Example 1: Asynchronous Kafka Producer with Non-Blocking Callbacks
```java
package com.example.platform.kafka.producer;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.kafka.core.KafkaTemplate;
import org.springframework.kafka.support.SendResult;
import org.springframework.stereotype.Service;

import java.util.concurrent.CompletableFuture;

public record OrderEvent(String orderId, String customerId, double amount, String status) {}

@Service
public class OrderEventProducer {

    private static final Logger log = LoggerFactory.getLogger(OrderEventProducer.class);
    private static final String TOPIC = "orders.v1";
    private final KafkaTemplate<String, OrderEvent> kafkaTemplate;

    public OrderEventProducer(KafkaTemplate<String, OrderEvent> kafkaTemplate) {
        this.kafkaTemplate = kafkaTemplate;
    }

    public void publishOrderEvent(OrderEvent event) {
        // Using orderId as key guarantees all events for this order hit the SAME partition
        CompletableFuture<SendResult<String, OrderEvent>> future =
                kafkaTemplate.send(TOPIC, event.orderId(), event);

        future.whenComplete((result, ex) -> {
            if (ex == null) {
                log.info("Order event [{}] delivered to topic [{}] partition [{}] at offset [{}]",
                        event.orderId(),
                        result.getRecordMetadata().topic(),
                        result.getRecordMetadata().partition(),
                        result.getRecordMetadata().offset());
            } else {
                log.error("Failed to deliver order event [{}] to Kafka: {}", event.orderId(), ex.getMessage(), ex);
                // Trigger alerting or store in fallback DB outbox
            }
        });
    }
}
```

### Example 2: Concurrent Consumer with Manual Offset Acknowledgment
```java
package com.example.platform.kafka.consumer;

import com.example.platform.kafka.producer.OrderEvent;
import org.apache.kafka.clients.consumer.ConsumerRecord;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.kafka.annotation.KafkaListener;
import org.springframework.kafka.support.Acknowledgment;
import org.springframework.stereotype.Component;

@Component
public class OrderEventConsumer {

    private static final Logger log = LoggerFactory.getLogger(OrderEventConsumer.class);

    // concurrency = "3" spawns 3 consumer threads processing partitions in parallel
    @KafkaListener(
            topics = "orders.v1",
            groupId = "order-fulfillment-group",
            concurrency = "3",
            containerFactory = "kafkaManualAckListenerContainerFactory"
    )
    public void consume(ConsumerRecord<String, OrderEvent> record, Acknowledgment acknowledgment) {
        log.info("Received event: Key={}, Partition={}, Offset={}, Payload={}",
                record.key(), record.partition(), record.offset(), record.value());

        try {
            processOrder(record.value());

            // Acknowledge offset strictly AFTER successful business processing
            if (acknowledgment != null) {
                acknowledgment.acknowledge();
            }
        } catch (Exception e) {
            log.error("Failed to process order event: {}. Offset NOT acknowledged.", record.value(), e);
            throw e; // Reraise to let DefaultErrorHandler handle retry/DLT
        }
    }

    private void processOrder(OrderEvent event) {
        // Business logic execution
    }
}
```

### Example 3: Container Factory with Manual Acknowledgment Mode
```java
package com.example.platform.kafka.config;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.kafka.config.ConcurrentKafkaListenerContainerFactory;
import org.springframework.kafka.core.ConsumerFactory;
import org.springframework.kafka.listener.ContainerProperties;

@Configuration
public class KafkaConsumerConfig {

    @Bean
    public ConcurrentKafkaListenerContainerFactory<String, Object> kafkaManualAckListenerContainerFactory(
            ConsumerFactory<String, Object> consumerFactory) {

        ConcurrentKafkaListenerContainerFactory<String, Object> factory =
                new ConcurrentKafkaListenerContainerFactory<>();
        factory.setConsumerFactory(consumerFactory);
        
        // Manual immediate commit mode: user code controls offset commits
        factory.getContainerProperties().setAckMode(ContainerProperties.AckMode.MANUAL_IMMEDIATE);
        
        return factory;
    }
}
```

### Example 4: Auto-Commit Data Loss Anti-Pattern and Fix
```java
// ANTI-PATTERN: Relying on enable.auto.commit = true.
// The consumer client commits offsets periodically in the background regardless of whether
// the application finishes processing or crashes midway, causing SILENT DATA LOSS!
// spring.kafka.consumer.enable-auto-commit=true // DANGEROUS!

// FIX: Disable auto-commit, use Spring AckMode.MANUAL_IMMEDIATE or RECORD
package com.example.platform.kafka.config;

import org.apache.kafka.clients.consumer.ConsumerConfig;
import org.springframework.boot.autoconfigure.kafka.KafkaProperties;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.kafka.core.DefaultKafkaConsumerFactory;

import java.util.Map;

@Configuration
public class SafeOffsetConsumerConfig {

    @Bean
    public DefaultKafkaConsumerFactory<String, Object> consumerFactory(KafkaProperties properties) {
        Map<String, Object> configs = properties.buildConsumerProperties(null);
        // Explicitly enforce disabling auto-commit to prevent message loss
        configs.put(ConsumerConfig.ENABLE_AUTO_COMMIT_CONFIG, false);
        return new DefaultKafkaConsumerFactory<>(configs);
    }
}
```

### Example 5: High-Performance Batch Consumer with Header Inspection
```java
package com.example.platform.kafka.consumer;

import com.example.platform.kafka.producer.OrderEvent;
import org.apache.kafka.clients.consumer.ConsumerRecords;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.kafka.annotation.KafkaListener;
import org.springframework.kafka.support.KafkaHeaders;
import org.springframework.messaging.handler.annotation.Header;
import org.springframework.stereotype.Component;

import java.util.List;

@Component
public class BatchOrderConsumer {

    private static final Logger log = LoggerFactory.getLogger(BatchOrderConsumer.class);

    // Batch consumption: receives up to max.poll.records (e.g. 500) items in a single method call
    @KafkaListener(
            topics = "orders.v1",
            groupId = "batch-analytics-group",
            batch = "true"
    )
    public void consumeBatch(
            List<OrderEvent> events,
            @Header(KafkaHeaders.RECEIVED_PARTITION) List<Integer> partitions,
            @Header(KafkaHeaders.OFFSET) List<Long> offsets) {

        log.info("Batch received: {} events across partitions {}", events.size(), partitions);
        // Bulk database insert: repository.saveAll(events)
    }
}
```

---

## 6. Best Practices

1. **Always Set `acks=all` (Producer)**: Ensure the leader broker replicates records to all in-sync replicas (ISRs) before acknowledging to eliminate data loss during broker crashes.
2. **Enable Producer Idempotence (`enable.idempotence=true`)**: Guarantees exactly-once message delivery from producer to partition, preventing duplicate messages caused by network retries.
3. **Partition by Domain Key**: Always specify a business key (e.g., `customerId`, `orderId`) so all events for that entity land in the same partition and are processed in strict order.
4. **Disable Auto-Commit (`enable.auto.commit=false`)**: Rely on Spring's managed acknowledgment modes (`RECORD`, `BATCH`, or `MANUAL_IMMEDIATE`) to ensure offsets are only committed after successful processing.
5. **Match Concurrency to Partition Count**: Never set `@KafkaListener(concurrency = N)` higher than the number of topic partitions; excess consumer threads remain permanently idle.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Setting `concurrency > partition count` | Excess consumer threads sit idle forever, wasting JVM thread resources | Ensure concurrency $\le$ number of topic partitions. |
| Blocking `poll()` loop with slow processing ($> max.poll.interval.ms$) | Kafka coordinator considers consumer dead, kicks it out, triggering constant rebalances | Offload slow processing or increase `max.poll.interval.ms`. |
| Poison pill JSON payloads causing infinite crash loops | Deserializer fails before reaching `@KafkaListener`, crashing container repeatedly | Configure `ErrorHandlingDeserializer` wrapping real serializers. |
| Missing partition key on ordered events | Messages distributed randomly (round-robin), destroying sequential event ordering | Always supply domain key (e.g., `orderId`) in `kafkaTemplate.send()`. |
| Synchronous `.get()` on KafkaTemplate future | Blocks the calling HTTP thread, destroying asynchronous throughput benefits | Use asynchronous callbacks: `future.whenComplete((res, ex) -> ...)`. |
| Committing offsets before business processing completes | If application crashes during processing, message is never reprocessed (data loss) | Use manual acknowledgment called *after* processing logic succeeds. |

---

## 8. Performance & Security Notes

- **Batching & Compression**: Configure `compression.type=snappy` (or `zstd`) and `linger.ms=20` on producers to dramatically reduce network bandwidth and increase throughput.
- **Consumer Rebalance Storms**: Long-running synchronous tasks during consumer execution exceed `max.poll.interval.ms`, causing group rebalances that halt consumption across all partitions.
- **SASL/SCRAM & TLS**: Secure cluster communication using SASL/SCRAM authentication and mutual TLS encryption over port 9093/9094.

---

## 9. Comparison / When to Use What

| Feature | Apache Kafka | RabbitMQ (AMQP) | Redis Streams |
|---|---|---|---|
| **Architecture** | Distributed Commit Log | Smart Broker / Dumb Consumer | In-memory append-only log |
| **Message Replay** | Arbitrary replay based on offset | Deleted upon consumer ACK | Replay via consumer groups |
| **Throughput** | Millions of events/sec | 50,000 - 100,000 msgs/sec | 500,000+ msgs/sec |
| **Routing** | Topic / Partition key hashing | Complex exchange routing (Topic/Fanout) | Stream key |
| **Persistence** | Durable multi-broker disk storage | Optional persistent queues | Memory + RDB/AOF |
| **Optimal For** | High-throughput streaming, Event Sourcing | Complex transactional routing, tasks | Low-latency in-memory streaming |

---

## 10. Interview Questions

<details>
<summary>1. How does Kafka guarantee strict message ordering?</summary>
Kafka guarantees message ordering *only within a single partition*. To ensure all events for a given domain entity (e.g., an order or user) are processed in order, producers must supply the same partition key (e.g., `orderId`). Kafka's default partitioner hashes the key to assign all matching records to the exact same partition.
</details>

<details>
<summary>2. What happens if the number of consumer instances in a consumer group exceeds the number of partitions?</summary>
Partitions are the maximum unit of parallelism in Kafka. If a topic has 4 partitions and a consumer group has 6 consumer instances, 4 consumers will be assigned 1 partition each, while 2 consumers will remain idle with no partitions assigned.
</details>

<details>
<summary>3. What is the role of ErrorHandlingDeserializer in Spring Kafka?</summary>
When a malformed message (poison pill) lands on a topic, standard deserializers throw exceptions during the `poll()` loop before the record reaches `@KafkaListener`, causing the listener container to crash and re-poll the same bad record infinitely. `ErrorHandlingDeserializer` catches deserialization errors and passes them as null or headers to the error handler, allowing poison pills to be routed to a dead-letter topic without halting the consumer.
</details>

<details>
<summary>4. What does `enable.idempotence=true` do on the Kafka Producer?</summary>
Producer idempotence prevents duplicate messages from being written to the broker when network retries occur. Kafka assigns each producer a unique Producer ID (PID) and tracks sequence numbers per partition. If a retry sends a sequence number the broker has already appended, the duplicate is discarded cleanly.
</details>

<details>
<summary>5. What is a Kafka consumer rebalance and why is it expensive?</summary>
A rebalance occurs when consumers join or leave a group, or when topic partitions change. The group coordinator revokes partition assignments and re-assigns them among active consumers. During eager rebalancing, all consumers stop processing messages ("stop-the-world"), introducing latency spikes. Modern Kafka mitigates this with Cooperative Sticky Assignors.
</details>

<details>
<summary>6. How does Spring's `Acknowledgment.acknowledge()` work with `AckMode.MANUAL_IMMEDIATE`?</summary>
When `AckMode.MANUAL_IMMEDIATE` is configured, calling `acknowledgment.acknowledge()` immediately invokes `consumer.commitSync()` or queues an asynchronous offset commit to Kafka for that specific record, ensuring exact user control over when progress is recorded.
</details>

<details>
<summary>7. What is `max.poll.interval.ms` and what happens if a listener takes longer to process?</summary>
`max.poll.interval.ms` is the maximum time allowed between calls to `consumer.poll()`. If `@KafkaListener` takes longer than this interval to finish processing a batch, the broker coordinator assumes the consumer thread has hung or died, kicks it out of the consumer group, and triggers a rebalance.
</details>

<details>
<summary>8. How do you configure Spring Kafka to publish and consume across multiple distinct Kafka clusters?</summary>
By declaring multiple distinct `ProducerFactory`, `ConsumerFactory`, `KafkaTemplate`, and `ConcurrentKafkaListenerContainerFactory` beans, each configured with separate bootstrap servers and credentials, and referencing the specific container factory in `@KafkaListener(containerFactory = "clusterBFactory")`.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1 (Easy): Create a simple Kafka producer</summary>
Write a Spring service that uses `KafkaTemplate<String, String>` to publish a text message to topic `"notifications"`.
</details>

<details>
<summary>Exercise 2 (Easy): Implement a basic @KafkaListener</summary>
Write a component that consumes messages from topic `"notifications"` within group `"notification-service-group"`.
</details>

<details>
<summary>Exercise 3 (Medium): Implement manual offset acknowledgment</summary>
Configure a listener using `AckMode.MANUAL_IMMEDIATE` that commits offsets only after persisting the record to a repository.
</details>

<details>
<summary>Exercise 4 (Medium): Configure ErrorHandlingDeserializer</summary>
Configure `application.properties` with `ErrorHandlingDeserializer` wrapping both key and value deserializers to prevent poison pill loops.
</details>

<details>
<summary>Exercise 5 (Hard): Implement partition concurrency and key-based distribution</summary>
Create a topic with 6 partitions. Configure a producer that publishes orders keyed by customer ID, and a listener with `concurrency = "6"` verifying that records for the same customer are handled sequentially.
</details>

<details>
<summary>Exercise 6 (Hard): Integration test with EmbeddedKafka / Testcontainers Kafka</summary>
Write a `@SpringBootTest` using `@EmbeddedKafka` that publishes a JSON event and verifies that an `@KafkaListener` successfully consumes and asserts the payload.
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                         Spring for Apache Kafka Summary                         |
+---------------------------------------------------------------------------------+
| Dependency:         org.springframework.kafka:spring-kafka                      |
| Producer:           KafkaTemplate.send(topic, key, payload) [CompletableFuture] |
| Producer Acks:      spring.kafka.producer.acks=all                              |
| Idempotence:        spring.kafka.producer.properties.enable.idempotence=true    |
| Consumer:           @KafkaListener(topics = "...", groupId = "...", concurrency = N)|
| Ack Mode:           MANUAL_IMMEDIATE with acknowledgment.acknowledge()          |
| Invariant:          Never set concurrency > partition count                     |
| Poison Pills:       Always wrap with ErrorHandlingDeserializer                  |
| Ordering Rule:      Strict ordering is guaranteed ONLY per partition via key    |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Official Spring for Apache Kafka Documentation](https://docs.spring.io/spring-kafka/reference/)
- [Apache Kafka Official Documentation](https://kafka.apache.org/documentation/)
- [Kafka: The Definitive Guide (Narkhede, Shapira, Palino)](https://www.oreilly.com/library/view/kafka-the-definitive/9781491936153/)
