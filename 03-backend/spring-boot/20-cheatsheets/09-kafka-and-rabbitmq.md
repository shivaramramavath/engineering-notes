# Spring Boot Kafka & RabbitMQ Cheatsheet

| Metadata | Details |
|---|---|
| **Topic** | Kafka & RabbitMQ Messaging Cheatsheet |
| **Track** | Java Spring Boot |
| **Difficulty** | Advanced |
| **Prerequisites** | Java 21, Event-Driven Architecture, AMQP, Kafka |
| **Java Version** | Java 21 / 25 LTS |
| **Spring Boot** | 3.4.x / 4.x |
| **Tested With** | Spring Kafka 3.3+, Spring AMQP 3.2+ |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [Internals & Architecture](#3-internals--architecture)
4. [Syntax & Categorized Reference](#4-syntax--categorized-reference)
5. [5 Practical Production Examples](#5-5-practical-production-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes to Avoid](#7-common-mistakes-to-avoid)
8. [Performance & Security Considerations](#8-performance--security-considerations)
9. [Comparison Table: Kafka vs RabbitMQ in Spring](#9-comparison-table-kafka-vs-rabbitmq-in-spring)
10. [Technical Interview Questions & Answers](#10-technical-interview-questions--answers)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading & References](#13-further-reading--references)

---

## 1. What & Why
Asynchronous messaging decouples distributed services, smooths traffic spikes, and powers reactive event-driven architectures. Spring for Apache Kafka (`spring-kafka`) and Spring AMQP / RabbitMQ (`spring-rabbit`) provide template abstractions, container factories, declarative listeners, manual acknowledgments, and dead-letter queue (DLQ) error handling.

This cheatsheet provides immediate syntax patterns for message producers, consumer listener containers, serialization codecs, and failover topologies.

---

## 2. Core Concepts
- **Spring Kafka**:
  - `KafkaTemplate<K, V>`: Thread-safe high-throughput publisher wrapping Apache Kafka Producer.
  - `@KafkaListener`: Declarative consumer bound to topic partitions and consumer groups.
  - `Acknowledgment`: Manual offset commit control (`AckMode.MANUAL_IMMEDIATE`).
  - `DefaultErrorHandler`: Configurable backoff retry and Dead Letter Publishing (`DeadLetterPublishingRecoverer`).
- **Spring AMQP / RabbitMQ**:
  - `RabbitTemplate`: Producer abstraction for AMQP exchange routing.
  - Declarative Topology: `@Bean` definitions of `Queue`, `TopicExchange`, and `Binding`.
  - `@RabbitListener`: Declarative AMQP consumer with automatic requeue/dead-letter policies.

---

## 3. Internals & Architecture
Spring Kafka & RabbitMQ Consumer Pipeline:

```
[Kafka Topic Partition / RabbitMQ Queue]
                    |
                    v
+-------------------------------------------------------------+
|               MessageListenerContainer                      |
|  - Kafka: ConcurrentMessageListenerContainer (Poll loop)    |
|  - RabbitMQ: SimpleMessageListenerContainer (AMQP Channel)  |
+-------------------------------------------------------------+
                    |
                    v
+-------------------------------------------------------------+
|              Record / Message Converter                     |
|  - String / JSON (ByteArrayJsonMessageConverter)            |
|  - Header extraction (CorrelationId, TypeId)                |
+-------------------------------------------------------------+
                    |
                    v
+-------------------------------------------------------------+
|             @KafkaListener / @RabbitListener                |
|               (Invokes Target Handler Method)               |
+-------------------------------------------------------------+
                    |
        +-----------+-----------+
        |                       |
   (Success)                 (Failure)
        |                       |
        v                       v
+--------------------+ +--------------------------------------+
| Acknowledgment     | | SeekToCurrentErrorHandler / DLQ      |
| Commit Offset /    | |  - Retries with ExponentialBackOff   |
| BasicAck           | |  - Publishes to .DLT / DLX on limit  |
+--------------------+ +--------------------------------------+
```

---

## 4. Syntax & Categorized Reference

### 4.1 Spring Kafka Essential Configuration (application.yaml)
```yaml
spring:
  kafka:
    bootstrap-servers: localhost:9092
    producer:
      key-serializer: org.apache.kafka.common.serialization.StringSerializer
      value-serializer: org.springframework.kafka.support.serializer.JsonSerializer
      acks: all
      retries: 3
      properties:
        enable.idempotence: true
    consumer:
      group-id: order-service-group
      auto-offset-reset: earliest
      enable-auto-commit: false
      key-deserializer: org.apache.kafka.common.serialization.StringDeserializer
      value-deserializer: org.springframework.kafka.support.serializer.ErrorHandlingDeserializer
      properties:
        spring.deserializer.value.delegate.class: org.springframework.kafka.support.serializer.JsonDeserializer
        spring.json.trusted.packages: "com.example.dto,com.example.events"
    listener:
      ack-mode: manual_immediate
```

### 4.2 Spring RabbitMQ Essential Configuration (application.yaml)
```yaml
spring:
  rabbitmq:
    host: localhost
    port: 5672
    username: guest
    password: guest
    listener:
      simple:
        acknowledge-mode: manual
        prefetch: 20
        default-requeue-rejected: false
```

---

## 5. 5 Practical Production Examples

### Example 1: Resilient Kafka Producer with CompletableFuture
```java
@Service
public class OrderEventPublisher {

    private final KafkaTemplate<String, OrderEvent> kafkaTemplate;

    public OrderEventPublisher(KafkaTemplate<String, OrderEvent> kafkaTemplate) {
        this.kafkaTemplate = kafkaTemplate;
    }

    public CompletableFuture<SendResult<String, OrderEvent>> publishOrderPlaced(OrderEvent event) {
        ProducerRecord<String, OrderEvent> record = new ProducerRecord<>("orders.placed", event.orderId().toString(), event);
        record.headers().add("X-Correlation-Id", UUID.randomUUID().toString().getBytes(StandardCharsets.UTF_8));

        return kafkaTemplate.send(record).whenComplete((result, ex) -> {
            if (ex != null) {
                // Log failure and alert
            } else {
                RecordMetadata metadata = result.getRecordMetadata();
                // Log successful publish with partition & offset
            }
        });
    }
}
```

### Example 2: Kafka Consumer with Manual Acknowledgment & Headers
```java
@Component
public class OrderPlacedConsumer {

    @KafkaListener(topics = "orders.placed", groupId = "inventory-service-group")
    public void handleOrderPlaced(
            @Payload OrderEvent event,
            @Header(KafkaHeaders.RECEIVED_PARTITION) int partition,
            @Header(KafkaHeaders.OFFSET) long offset,
            Acknowledgment acknowledgment) {
        try {
            processInventoryAllocation(event);
            acknowledgment.acknowledge(); // Explicitly commit offset on success
        } catch (TransientException ex) {
            // Let exception bubble up to trigger DefaultErrorHandler retry
            throw ex;
        }
    }

    private void processInventoryAllocation(OrderEvent event) {
        // Business logic
    }
}
```

### Example 3: Kafka Dead Letter Queue (DLT) Retry Configuration
```java
@Configuration
public class KafkaRetryConfig {

    @Bean
    public DefaultErrorHandler defaultErrorHandler(KafkaTemplate<String, Object> template) {
        DeadLetterPublishingRecoverer recoverer = new DeadLetterPublishingRecoverer(template,
                (record, ex) -> new TopicPartition(record.topic() + ".DLT", record.partition()));

        ExponentialBackOffWithMaxRetries backOff = new ExponentialBackOffWithMaxRetries(3);
        backOff.setInitialInterval(1000L);
        backOff.setMultiplier(2.0);
        backOff.setMaxInterval(10000L);

        DefaultErrorHandler errorHandler = new DefaultErrorHandler(recoverer, backOff);
        errorHandler.addNotRetryableExceptions(IllegalArgumentException.class);
        return errorHandler;
    }
}
```

### Example 4: Declarative RabbitMQ Topology Definition
```java
@Configuration
public class RabbitMqTopologyConfig {

    public static final String EXCHANGE_NAME = "orders.exchange";
    public static final String QUEUE_NAME = "orders.notifications.queue";
    public static final String ROUTING_KEY = "orders.created.*";

    @Bean
    public TopicExchange orderExchange() {
        return new TopicExchange(EXCHANGE_NAME, true, false);
    }

    @Bean
    public Queue orderNotificationQueue() {
        return QueueBuilder.durable(QUEUE_NAME)
                .withArgument("x-dead-letter-exchange", "orders.dlx")
                .withArgument("x-dead-letter-routing-key", "orders.dead-letter")
                .build();
    }

    @Bean
    public Binding binding(Queue orderNotificationQueue, TopicExchange orderExchange) {
        return BindingBuilder.bind(orderNotificationQueue).to(orderExchange).with(ROUTING_KEY);
    }
}
```

### Example 5: RabbitMQ Consumer with Channel Manual Ack
```java
@Component
public class NotificationAmqpConsumer {

    @RabbitListener(queues = RabbitMqTopologyConfig.QUEUE_NAME)
    public void processNotification(
            @Payload NotificationMessage message,
            Channel channel,
            @Header(AmqpHeaders.DELIVERY_TAG) long deliveryTag) throws IOException {
        try {
            sendPushNotification(message);
            channel.basicAck(deliveryTag, false);
        } catch (Exception ex) {
            // Reject message and do not requeue -> routes directly to Dead Letter Exchange (DLX)
            channel.basicNack(deliveryTag, false, false);
        }
    }

    private void sendPushNotification(NotificationMessage message) {
        // Notification logic
    }
}
```

---

## 6. Best Practices
1. **Enable Idempotence on Kafka Producers**: Set `enable.idempotence=true` and `acks=all` for exactly-once producer delivery.
2. **Always Use Manual Acknowledgments**: Prevent data loss when processing fails after message consumption.
3. **Use ErrorHandlingDeserializer**: Prevents poisoned messages (invalid JSON payloads) from blocking the Kafka poll loop indefinitely.
4. **Configure Explicit Prefetch on RabbitMQ**: Avoid prefetching unbounded messages (`prefetch: 10-50`) to prevent memory exhaustion.
5. **Implement DLQ Topics for Poison Pills**: Route unrecoverable messages to `.DLT` for manual triage and replay.

---

## 7. Common Mistakes to Avoid

| Mistake | Consequence | Remedy |
|---|---|---|
| Auto-commit enabled in Kafka (`enable-auto-commit: true`) | Offsets committed before processing completes; failures cause message loss | Set `enable-auto-commit: false` and use `AckMode.MANUAL_IMMEDIATE` |
| Missing `ErrorHandlingDeserializer` | Bad JSON payload stalls consumer permanently with serialization loop | Wrap deserializer in `ErrorHandlingDeserializer` |
| Infinite retry on non-transient exceptions | Malformed payloads loop forever, halting partition consumption | Add non-retryable exceptions to `DefaultErrorHandler` |
| Calling blocking methods inside producer callbacks | Slows down Kafka sender thread, causing backpressure | Offload heavy work to asynchronous application thread pool |
| Setting RabbitMQ requeue=true on parse failures | Repeatedly rejects and re-queues bad message at infinite rate | Set `requeue=false` and configure Dead Letter Exchange |
| Not specifying `trusted.packages` in JsonSerializer | Security exception `SecurityException: Package not trusted` | Specify package prefix in `spring.json.trusted.packages` |

---

## 8. Performance & Security Considerations
- **Kafka Partition Sizing**: Ensure partition keys distribute evenly across topic partitions to prevent partition hotspots.
- **Batch Processing**: Use `@KafkaListener(batch = "true")` and `List<ConsumerRecord>` for high-volume ingest (100k+ msg/sec).
- **TLS & SASL Encryption**: Protect messaging traffic with SSL and SASL_SSL / SCRAM-SHA-512 credentials in production.

---

## 9. Comparison Table: Kafka vs RabbitMQ in Spring

| Metric | Apache Kafka (`spring-kafka`) | RabbitMQ (`spring-rabbit`) |
|---|---|---|
| **Architecture** | Distributed Commit Log | Message Broker / AMQP Exchange |
| **Ordering** | Strict per-partition ordering | FIFO per queue (broken by multiple consumers) |
| **Retention** | Time/Size-based (replayable) | Transient (deleted after acknowledgment) |
| **Throughput** | Millions of messages/sec | Tens of thousands of messages/sec |
| **Routing Flexibility** | Topic / Partition key | Complex (Direct, Fanout, Topic, Headers) |

---

## 10. Technical Interview Questions & Answers

<details>
<summary>1. How does Spring Kafka prevent poison pills from blocking a partition?</summary>
By configuring `ErrorHandlingDeserializer`. When deserialization fails, it delegates to an error handler rather than throwing a deserialization exception in the poll loop, allowing the record to be sent directly to the Dead Letter Topic.
</details>

<details>
<summary>2. What is the difference between `AckMode.RECORD` and `AckMode.MANUAL_IMMEDIATE`?</summary>
`RECORD` commits the offset automatically after each individual record listener returns. `MANUAL_IMMEDIATE` gives control to the developer via the `Acknowledgment` object to commit the offset immediately on demand.
</details>

<details>
<summary>3. What is consumer rebalancing in Kafka and how does Spring handle it?</summary>
Rebalancing happens when consumers join or leave a group. Spring Kafka's `ConsumerAwareRebalanceListener` allows applications to flush pending state or commit offsets before partitions are revoked.
</details>

<details>
<summary>4. How does RabbitMQ Dead Letter Exchange (DLX) work?</summary>
When a queue declares `x-dead-letter-exchange`, any message rejected with `basicNack(false, false)` or expired by TTL is automatically forwarded by the broker to the configured DLX.
</details>

<details>
<summary>5. What does `enable.idempotence=true` ensure on a Kafka Producer?</summary>
It guarantees that duplicate network retries by the producer do not result in duplicate records in the log, assigning a Producer ID (PID) and sequence numbers to every batch.
</details>

<details>
<summary>6. How do you scale consumer processing in Spring Kafka?</summary>
Increase the `concurrency` property on `@KafkaListener` or `ConcurrentKafkaListenerContainerFactory` up to the total number of partitions available in the topic.
</details>

<details>
<summary>7. What is RabbitMQ `prefetch` count and why is it critical?</summary>
`prefetch` limits how many unacknowledged messages the broker sends to a consumer. Without it, the broker dumps all queue messages into consumer memory, causing OOM.
</details>

<details>
<summary>8. How do you implement distributed tracing across Kafka messages?</summary>
Spring Cloud Sleuth / Micrometer Tracing injects trace and span IDs into Kafka `RecordHeaders` on produce, which the listener extracts into the local trace context on consume.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary>Exercise 1: Create an idempotent Kafka publisher</summary>
Write a service sending user registration events with message keys and correlation headers.
</details>

<details>
<summary>Exercise 2: Implement a Kafka consumer with manual ack and DLT</summary>
Write a listener that commits offsets on success and configures automatic routing to `.DLT` on failure.
</details>

<details>
<summary>Exercise 3: Declare RabbitMQ topic exchange topology</summary>
Define beans for a TopicExchange, two bound queues (`audit.queue`, `analytics.queue`), and routing keys.
</details>

<details>
<summary>Exercise 4: Test Kafka messaging with Testcontainers</summary>
Write an integration test using Testcontainers Kafka, producing a message and verifying consumption.
</details>

<details>
<summary>Exercise 5: Implement batch Kafka consumer</summary>
Configure a listener container factory with `batchListener=true` and process a batch of 50 records in a single transaction.
</details>

<details>
<summary>Exercise 6: Configure RabbitMQ Dead Letter Exchange</summary>
Set up an order processing queue that diverts failed messages to `order.dlx` without requeue.
</details>

---

## 12. Summary & Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                   KAFKA & RABBITMQ CHEAT SHEET                                  |
+---------------------------------------------------------------------------------+
| KAFKA PRODUCER | kafkaTemplate.send(new ProducerRecord<>(topic, key, payload))   |
| KAFKA CONSUMER | @KafkaListener(topics = "topic", groupId = "group")            |
| KAFKA ACK      | acknowledgment.acknowledge() (AckMode.MANUAL_IMMEDIATE)        |
| KAFKA DLQ      | DefaultErrorHandler with DeadLetterPublishingRecoverer          |
| RABBIT TOPOLOGY| @Bean TopicExchange, @Bean Queue, @Bean BindingBuilder          |
| RABBIT LISTENER| @RabbitListener(queues = "queueName")                          |
| RABBIT ACK     | channel.basicAck(deliveryTag, false) / basicNack(..., false)   |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading & References
- [Spring for Apache Kafka Documentation](https://docs.spring.io/spring-kafka/reference/)
- [Spring AMQP (RabbitMQ) Documentation](https://docs.spring.io/spring-amqp/reference/)
- [Confluent Kafka Production Best Practices](https://docs.confluent.io/platform/current/kafka/deployment.html)
- [RabbitMQ Reliability Guide](https://www.rabbitmq.com/reliability.html)
