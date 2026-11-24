# Module 11: Asynchronous Processing, Queues & Distributed Events

> Distributed job scheduling, message brokers, BullMQ v5, RabbitMQ, Kafka, and resilient data consistency patterns.

---

## Module Overview

Node.js servers excel at delegating heavy or long-running work to asynchronous background queues. This module covers queue architectures, retry backoffs, dead letter queues (DLQs), distributed pub/sub brokers (RabbitMQ, Kafka), and distributed consistency patterns like the Transactional Outbox and Sagas.

---

## Contents

| Index | Note | Core Focus |
| :---: | :--- | :--- |
| **01** | [01-queues-and-bullmq.md](./01-queues-and-bullmq.md) | BullMQ v5 fundamentals, Redis connection options, jobs and workers. |
| **02** | [02-workers-retry-dlq.md](./02-workers-retry-dlq.md) | Worker concurrency, exponential backoff retries, Dead Letter Queues (DLQ). |
| **03** | [03-scheduled-jobs.md](./03-scheduled-jobs.md) | Distributed cron, delayed jobs, recurring task execution with BullMQ. |
| **04** | [04-kafka.md](./04-kafka.md) | Apache Kafka with KafkaJS: partitions, consumer groups, offsets. |
| **05** | [05-rabbitmq-and-amqplib.md](./05-rabbitmq-and-amqplib.md) | RabbitMQ topic/direct exchanges, publisher confirms, prefetch settings. |
| **06** | [06-outbox-and-saga-patterns.md](./06-outbox-and-saga-patterns.md) | Solving dual-writes with Transactional Outbox, distributed Sagas. |
| **07** | [07-idempotent-consumers-and-stream-processing.md](./07-idempotent-consumers-and-stream-processing.md) | Idempotent consumers with Redis locks, at-least-once delivery guarantees. |

---

## Next Steps
Proceed to [12-realtime](../12-realtime/README.md) to explore WebSockets, Socket.IO clusters, WebRTC signaling, and realtime backpressure.
