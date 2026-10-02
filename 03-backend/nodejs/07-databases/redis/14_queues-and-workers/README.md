# 14 · Queues and Workers

A queue moves slow, unreliable or bursty work **out of the request path** and into background workers. Redis is the most common backing store for job queues in Node.js: it is fast, atomic, and has the right data structures (lists, sorted sets, streams, Lua).

This module builds a small queue **from scratch first**, so you understand leases, delays, retries and dead letters, and then shows **BullMQ**, the production library that implements all of it for you.

## Lessons

| # | Lesson | Core idea |
|---|--------|-----------|
| 01 | [Queue Fundamentals](./01_queue-fundamentals.md) | Job lifecycle, delivery guarantees, a reliable Redis queue with leases and a stalled-job reaper |
| 02 | [Delayed Jobs](./02_delayed-jobs.md) | Sorted-set scheduling, a safe promoter, cancel and reschedule, recurring jobs |
| 03 | [Retries and Dead-Letter Queue](./03_retries-and-dead-letter-queue.md) | Failure classes, backoff with jitter, poison jobs, replaying dead letters |
| 04 | [BullMQ](./04_bullmq.md) | The production library: options, workers, flows, schedulers, operations |

## Learning outcomes

After this module you can:

- Choose the right Redis structure for a queue and explain its delivery guarantee
- Build a reliable worker with leases, heartbeats, graceful shutdown and stalled-job recovery
- Schedule, cancel and repeat jobs without cron-in-every-instance bugs
- Design retry policies that survive outages without creating retry storms
- Run a dead-letter queue as a real operational tool
- Use BullMQ confidently, and know when to use Streams or a bigger broker instead

## The mental model

```
producer ──add──►  [ waiting ] ──claim──► [ active ] ──ack──► completed
                        ▲                     │  │
                        │                     │  └─fail(retryable)──► [ delayed ] ──(time passes)──┐
                        └─────(promote)───────┼───────────────────────────────────────────────────┘
                                              └─fail(permanent / attempts exhausted)──► [ dead / failed ]
   delayed jobs ──► [ delayed ] ──(time passes)──► waiting
   worker died? the lease expires ──► a reaper puts the job back in waiting
```

Every job is in exactly one state. Every transition is **atomic** (a Lua script), so a job is never lost or processed by two workers because of a race.

## Delivery guarantees

| Guarantee | Meaning | How you get it |
|-----------|---------|----------------|
| **At-most-once** | Never duplicated, can be lost | `BRPOP` and process. A crash loses the job |
| **At-least-once** | Never lost, **can run twice** | Claim with a **lease**, acknowledge after success, requeue on expiry |
| **Exactly-once** | Doesn't exist as a transport property | At-least-once **plus idempotent handlers** |

Everything in this module is **at-least-once**. Your handlers must tolerate being run twice ([idempotency](../10_streams/04_stream-patterns.md#idempotent-consumers)).

## Which tool?

| Need | Use |
|------|-----|
| A real production job system: retries, delays, priorities, rate limits, dashboards | **BullMQ** (lesson 04) |
| A simple fire-and-forget background task, loss acceptable | A Redis list with `BRPOP` |
| Durable events, several services each reading everything, replay | [Streams](../10_streams/README.md) |
| Notifications where loss is fine | [Pub/Sub](../09_pub-sub/README.md) |
| Huge volume, very long retention, many teams | Kafka, SQS, RabbitMQ and similar |
| Understanding how all of this works | Lessons 01 to 03 |

## Prerequisites

- Completed [13_session-management](../13_session-management/README.md)
- Comfortable with [Lua scripts](../06_advanced-commands/03_lua-scripts.md), [Streams](../10_streams/README.md), [blocking connections](../08_nodejs-integration/01_connection-management.md) and [distributed locks](../11_distributed-locks/README.md)

## Next

Continue to [15_redis-architecture](../15_redis-architecture/README.md).
