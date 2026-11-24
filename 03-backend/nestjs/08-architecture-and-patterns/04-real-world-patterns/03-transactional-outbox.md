---
title: "The Transactional Outbox Pattern in NestJS"
description: "Solving the dual-write problem: guaranteed at-least-once message delivery using database outbox tables and background polling"
tags: [nestjs, patterns, transactional-outbox, dual-write, event-driven, kafka, rabbitmq]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# The Transactional Outbox Pattern in NestJS

## Conceptual Overview
When an application updates a database AND publishes a message to a broker (Kafka, RabbitMQ), a network or process crash between the two operations causes inconsistency (the **Dual-Write Problem**):
- If DB commits and Broker publish fails $	o$ Event lost forever.
- If Broker publishes and DB rollback occurs $	o$ Phantom event dispatched.

The **Transactional Outbox Pattern** saves outgoing messages to a dedicated `outbox` table in the **same atomic database transaction** as the business mutation. A separate worker or Change Data Capture (CDC via Debezium) publishes outbox records to the broker.

```
+-------------------------------------------------------------------------------+
|                       Transactional Outbox Architecture                       |
|                                                                               |
|  [ OrdersService ]                                                            |
|         |                                                                     |
|         | Single ACID Transaction                                             |
|         v                                                                     |
|  [ PostgreSQL ]                                                               |
|  - INSERT INTO orders (...)                                                   |
|  - INSERT INTO outbox (topic, payload, status = 'PENDING')                    |
|         |                                                                     |
|  Commit Transaction                                                           |
|         |                                                                     |
|         v                                                                     |
|  [ Outbox Relayer / BullMQ Worker ]                                           |
|    - SELECT * FROM outbox WHERE status = 'PENDING' FOR UPDATE SKIP LOCKED      |
|    - Publish to Kafka / RabbitMQ broker                                       |
|    - UPDATE outbox SET status = 'PUBLISHED'                                   |
+-------------------------------------------------------------------------------+
```

---

## Key Takeaways & Cheat Sheet

- Never publish to external message brokers inside database transactions without an outbox.
- Save outbox messages in the same ACID transaction as the domain entity update.
- Use `FOR UPDATE SKIP LOCKED` or Debezium CDC for high-performance outbox polling.

---

## Cross-References
- [04-saga.md](./04-saga.md) - Distributed sagas
- [06-broker-transporters.md](../../05-advanced/06-microservices/04-broker-transporters.md) - Message brokers
