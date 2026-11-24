# Module 10: Background Processing & Distributed Messaging

Welcome to **Module 10: Background Processing & Distributed Messaging**. This module explores asynchronous background job distribution across Celery, Taskiq, Arq, Redis Streams, RabbitMQ, and Apache Kafka.

---

## Module Overview

| File | Title | Description |
| :--- | :--- | :--- |
| [01-background-tasks-vs-task-queues.md](./01-background-tasks-vs-task-queues.md) | **In-Process vs Distributed Queues**| When to use Starlette BackgroundTasks vs external brokers. |
| [02-celery-with-fastapi.md](./02-celery-with-fastapi.md) | **Celery 5.7 Integration** | Configuring Celery workers, brokers, and result backends. |
| [03-arq-dramatiq-and-taskiq.md](./03-arq-dramatiq-and-taskiq.md) | **Async-Native Task Queues** | Taskiq and Arq for modern asyncio-native background tasks. |
| [04-rq-and-lightweight-options.md](./04-rq-and-lightweight-options.md) | **RQ & Lightweight Workers** | Simple Redis Queue options for small-scale operations. |
| [05-retries-dead-letters-idempotency.md](./05-retries-dead-letters-idempotency.md) | **Retries, DLQs & Idempotency** | Exponential backoff, poison pill queues, and safe replays. |
| [06-scheduled-jobs-with-apscheduler.md](./06-scheduled-jobs-with-apscheduler.md) | **Scheduled & Cron Jobs** | Periodic task scheduling with APScheduler and Celery Beat. |
| [07-kafka-with-python-clients.md](./07-kafka-with-python-clients.md) | **Apache Kafka Integration** | Producing and consuming high-throughput event streams with `aiokafka`. |
| [08-rabbitmq-with-aio-pika.md](./08-rabbitmq-with-aio-pika.md) | **RabbitMQ with aio-pika** | AMQP exchanges, routing keys, work queues, and publisher confirms. |
| [09-redis-streams-and-pubsub.md](./09-redis-streams-and-pubsub.md) | **Redis Streams & Pub/Sub** | Consumer groups, stream offsets, and in-memory broadcasting. |
| [10-outbox-and-saga-patterns.md](./10-outbox-and-saga-patterns.md) | **Outbox & Saga Patterns** | Preventing dual writes and orchestrating distributed transactions. |
| [11-websockets-at-scale-with-redis.md](./11-websockets-at-scale-with-redis.md) | **WebSocket Clustering** | Scaling WebSockets across multi-node Kubernetes clusters via Redis. |
| [12-server-sent-events-at-scale.md](./12-server-sent-events-at-scale.md) | **SSE at Scale** | Distributing SSE event channels across stateless API instances. |
| [13-event-driven-design.md](./13-event-driven-design.md) | **Event-Driven Architecture** | Designing event schemas, topic topologies, and event evolution. |
| [14-job-observability.md](./14-job-observability.md) | **Queue Observability** | Monitoring worker queue lag, failure metrics, and Flower dashboard. |
