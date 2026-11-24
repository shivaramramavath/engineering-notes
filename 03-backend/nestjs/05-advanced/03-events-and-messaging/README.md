# 05-03 — Events, Messaging, and CQRS

Enterprise backends rely on event-driven communication to eliminate tight coupling, scale asynchronous operations, and establish resilient microservice boundaries.

This module covers the entire spectrum of event architectures: in-process Pub/Sub via **`@nestjs/event-emitter`**, distributed message brokers (**Apache Kafka** and **RabbitMQ** via `@nestjs/microservices`), and Domain-Driven Design architectures with **CQRS** (`CommandBus`, `QueryBus`, `EventBus`, and Sagas).

---

## Module Index

| # | File | Core Topics |
|---|---|---|
| 01 | [Event Emitter](./01-event-emitter.md) | `@nestjs/event-emitter`, `EventEmitter2`, wildcards, async listeners, transactional boundaries |
| 02 | [Messaging Fundamentals](./02-messaging-fundamentals.md) | Request-Response (RPC) vs Event-Driven (Pub/Sub), Smart vs Dumb brokers, `ClientProxy` |
| 03 | [Apache Kafka](./03-kafka.md) | Distributed commit logs, topics, partitions, consumer groups, message keys, `.unwrap()` |
| 04 | [RabbitMQ](./04-rabbitmq.md) | AMQP 0-9-1, Exchanges (Direct, Topic, Fanout), manual ACK/NACK, Dead-Letter Exchanges |
| 05 | [CQRS Module](./05-cqrs-module.md) | `@nestjs/cqrs`, CommandBus, QueryBus, EventBus, AggregateRoot, Sagas with RxJS |

---

## Key Learning Outcomes

1. Decouple monolith features cleanly using in-process event buses.
2. Select the right messaging broker (Kafka for high-throughput append-logs; RabbitMQ for complex transactional routing).
3. Implement resilient microservices with manual acknowledgments and dead-letter queues.
4. Scale high-volume event streams using partition keys and consumer groups in Kafka.
5. Separate command mutations from read queries using the CQRS pattern and reactive Sagas.
