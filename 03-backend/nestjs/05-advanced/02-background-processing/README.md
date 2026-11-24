# 05-02 — Background Processing and Schedulers

Decoupling long-running, resource-intensive, or delayed tasks from the client-facing HTTP lifecycle is essential for building scalable, responsive backend systems.

This module covers both distributed queue systems (**BullMQ** over Redis) and in-process task schedulers (**`@nestjs/schedule`**), providing complete production architectures for worker scaling, error recovery, and dynamic scheduling.

---

## Module Index

| # | File | Core Topics |
|---|---|---|
| 01 | [Queue Fundamentals](./01-queues-fundamentals.md) | Producer-consumer architecture, at-least-once delivery, event loop decoupling |
| 02 | [BullMQ Integration](./02-bullmq.md) | `@Processor`, `@Process`, `WorkerHost`, Redis Streams, lock renewal |
| 03 | [Producers & Consumers](./03-producers-and-consumers.md) | Typed payloads, progress updates, parent-child Flows |
| 04 | [Retries, Backoff & DLQ](./04-retries-and-dead-letter-queues.md) | Exponential backoff, `UnrecoverableError`, Dead-Letter Queue management |
| 05 | [Delayed & Repeatable Jobs](./05-delayed-and-repeatable-jobs.md) | Redis ZSET delays, distributed repeatable cron schedules, cancellation |
| 06 | [Job Idempotency & Deduplication](./06-job-idempotency.md) | Deterministic `jobId`, atomic SQL `processed_jobs` locking |
| 07 | [Queue Scaling & Performance](./07-queue-scaling-and-performance.md) | Sandboxed child processes, Worker Threads, KEDA autoscaling, Prometheus metrics |
| 08 | [Scheduling with Cron](./08-scheduling-with-cron.md) | `@Cron()`, `@Interval()`, `@Timeout()`, distributed lock guards |
| 09 | [Dynamic Scheduling](./09-dynamic-scheduling-and-cron-job-management.md) | `SchedulerRegistry`, runtime job registration, database-driven schedules |

---

## Key Learning Outcomes

1. Decouple slow operations from HTTP controllers to return sub-10ms responses.
2. Build resilient background consumers with automatic exponential backoff retries.
3. Eliminate duplicate billing or task executions with two-tier idempotency.
4. Scale CPU-intensive workers using sandboxed child processes and KEDA metrics.
5. Manage both static and dynamic recurring schedules across multi-pod environments.
