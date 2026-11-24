# Module 10: Messaging & Event-Driven Systems

Welcome to **Module 10: Messaging & Event-Driven Systems**. This module covers asynchronous messaging and stream processing in Spring Boot: Spring ApplicationEvents with `@TransactionalEventListener`, Spring for Apache Kafka, RabbitMQ (Spring AMQP), Spring Cloud Stream, the Transactional Outbox pattern, idempotent consumers, dead-letter queues, Spring Batch, and Spring Modulith.

---

## Module Contents

| # | Note | Description | Level | Status |
|---|---|---|---|---|
| 01 | [01-spring-events-and-transactional-events.md](file:///03-backend/spring-boot/10-messaging-and-events/01-spring-events-and-transactional-events.md) | In-process domain events, `@TransactionalEventListener(phase = AFTER_COMMIT)` | Intermediate | Complete |
| 02 | [02-spring-kafka.md](file:///03-backend/spring-boot/10-messaging-and-events/02-spring-kafka.md) | `KafkaTemplate`, `@KafkaListener`, consumer concurrency, partition offsets | Advanced | Complete |
| 03 | [03-spring-amqp-rabbitmq.md](file:///03-backend/spring-boot/10-messaging-and-events/03-spring-amqp-rabbitmq.md) | `RabbitTemplate`, Direct/Topic/Fanout exchanges, message acknowledgments | Intermediate | Complete |
| 04 | [04-spring-cloud-stream.md](file:///03-backend/spring-boot/10-messaging-and-events/04-spring-cloud-stream.md) | Binder abstraction for Kafka and RabbitMQ using Java functional interfaces | Advanced | Complete |
| 05 | [05-outbox-and-saga.md](file:///03-backend/spring-boot/10-messaging-and-events/05-outbox-and-saga.md) | Implementing the Transactional Outbox pattern and distributed Saga orchestration | Advanced | Complete |
| 06 | [06-idempotent-consumers.md](file:///03-backend/spring-boot/10-messaging-and-events/06-idempotent-consumers.md) | Preventing duplicate message side-effects using Redis idempotency deduplication | Advanced | Complete |
| 07 | [07-dead-letter-and-retry.md](file:///03-backend/spring-boot/10-messaging-and-events/07-dead-letter-and-retry.md) | Dead-letter topics (`.DLT`), exponential backoff retries, poison pill isolation | Advanced | Complete |
| 08 | [08-spring-integration-overview.md](file:///03-backend/spring-boot/10-messaging-and-events/08-spring-integration-overview.md) | Enterprise Integration Patterns (EIP), message channels, transformers, filters | Advanced | Complete |
| 09 | [09-spring-batch-fundamentals.md](file:///03-backend/spring-boot/10-messaging-and-events/09-spring-batch-fundamentals.md) | Spring Batch 5+ architecture: Job, Step, ItemReader, ItemProcessor, ItemWriter | Intermediate | Complete |
| 10 | [10-spring-batch-in-production.md](file:///03-backend/spring-boot/10-messaging-and-events/10-spring-batch-in-production.md) | Chunk-oriented processing, skip/retry policies, job restarts, metadata tables | Advanced | Complete |
| 11 | [11-spring-modulith.md](file:///03-backend/spring-boot/10-messaging-and-events/11-spring-modulith.md) | Modular monoliths, architectural boundary verification, persistent outbox events | Advanced | Complete |

---

## Next Steps
Proceed to **[Module 11: Cloud & Microservices](file:///03-backend/spring-boot/11-cloud-and-microservices/)** to explore Spring Cloud Gateway, Resilience4j, and distributed tracing.
